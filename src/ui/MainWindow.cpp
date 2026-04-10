#include "ui/MainWindow.h"
#include <QStatusBar>
#include <QButtonGroup>
#include <QApplication>
#include <QFile>
#include <QInputDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QHeaderView>
#include <QScrollBar>
#include <QFrame>
#include <QFont>
#include <QTimer>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    db_ = new FarhanDB::DBBridge();
    setupUI();
    applyTheme();
    setWindowTitle("FarhanDB v2.0.0");
    setMinimumSize(1200, 750);
    resize(1400, 850);
}

MainWindow::~MainWindow() {
    delete db_;
}

void MainWindow::setupUI() {
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    setupTopBar();
    rootLayout->addWidget(topBar_);

    // Main splitter: sidebar + content
    mainSplitter_ = new QSplitter(Qt::Horizontal);
    mainSplitter_->setHandleWidth(1);

    setupSidebar();
    mainSplitter_->addWidget(sidebar_);

    // Tab widget for modes
    tabWidget_ = new QTabWidget;
    tabWidget_->setObjectName("modeTab");
    tabWidget_->setTabPosition(QTabWidget::North);

    setupSQLMode();
    setupBeginnerMode();
    setupServerMode();

    tabWidget_->addTab(sqlModeWidget_,      "⌨  SQL Mode");
    tabWidget_->addTab(beginnerWidget_,     "🟢  Beginner Mode");
    tabWidget_->addTab(serverWidget_,       "🌐  Server Mode");

    mainSplitter_->addWidget(tabWidget_);
    mainSplitter_->setStretchFactor(0, 0);
    mainSplitter_->setStretchFactor(1, 1);
    mainSplitter_->setSizes({230, 1000});

    rootLayout->addWidget(mainSplitter_);

    // Status bar
    statusBar()->setObjectName("statusBar");
    statusBar()->showMessage("✅  Ready  •  FarhanDB v2.0.0  •  Database: " +
        QString::fromStdString(db_->CurrentDatabase()));
}

void MainWindow::setupTopBar() {
    topBar_ = new QWidget;
    topBar_->setObjectName("topBar");
    topBar_->setFixedHeight(50);

    auto* tl = new QHBoxLayout(topBar_);
    tl->setContentsMargins(20, 0, 20, 0);
    tl->setSpacing(8);

    // Logo text
    auto* logoText = new QLabel("FarhanDB");
    logoText->setObjectName("appTitle");

    // Version badge - no inline style, let QSS handle it
    auto* versionBadge = new QLabel("v2.0.0");
    versionBadge->setObjectName("versionBadge");

    tl->addWidget(logoText);
    tl->addWidget(versionBadge);
    tl->addStretch();

    // DB label - centered
    dbLabel_ = new QLabel;
    dbLabel_->setObjectName("dbLabel");
    updateDbLabel();
    tl->addWidget(dbLabel_);
    tl->addStretch();

    // Theme button
    themeBtn_ = new QPushButton("Dark Mode");
    themeBtn_->setObjectName("themeButton");
    themeBtn_->setFixedSize(110, 32);
    themeBtn_->setCursor(Qt::PointingHandCursor);
    tl->addWidget(themeBtn_);

    connect(themeBtn_, &QPushButton::clicked, this, &MainWindow::toggleTheme);
}

void MainWindow::setupSidebar() {
    sidebar_ = new QWidget;
    sidebar_->setObjectName("sidePanel");
    sidebar_->setMinimumWidth(220);
    sidebar_->setMaximumWidth(260);

    auto* sl = new QVBoxLayout(sidebar_);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);

    // Sidebar header
    auto* sideHeader = new QWidget;
    sideHeader->setObjectName("sideHeader");
    sideHeader->setFixedHeight(40);
    auto* shl = new QHBoxLayout(sideHeader);
    shl->setContentsMargins(10, 0, 10, 0);
    auto* explorerLabel = new QLabel("📂  Explorer");
    explorerLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    refreshBtn_ = new QPushButton("↻");
    refreshBtn_->setFixedSize(24, 24);
    refreshBtn_->setToolTip("Refresh");
    refreshBtn_->setCursor(Qt::PointingHandCursor);
    shl->addWidget(explorerLabel);
    shl->addStretch();
    shl->addWidget(refreshBtn_);
    sl->addWidget(sideHeader);

    // Tree
    sideTree_ = new QTreeWidget;
    sideTree_->setObjectName("sideTree");
    sideTree_->setHeaderHidden(true);
    sideTree_->setAnimated(true);
    sideTree_->setIndentation(16);
    sideTree_->setExpandsOnDoubleClick(false);
    sl->addWidget(sideTree_);

    // DB action buttons
    auto* dbActions = new QWidget;
    dbActions->setObjectName("dbActions");
    auto* dal = new QVBoxLayout(dbActions);
    dal->setContentsMargins(8, 8, 8, 8);
    dal->setSpacing(6);

    newDbBtn_    = new QPushButton("➕  New Database");
    switchDbBtn_ = new QPushButton("🔄  Switch Database");
    dropDbBtn_   = new QPushButton("🗑  Drop Database");

    newDbBtn_->setObjectName("sideBtn");
    switchDbBtn_->setObjectName("sideBtn");
    dropDbBtn_->setObjectName("sideBtnDanger");

    for (auto* btn : {newDbBtn_, switchDbBtn_, dropDbBtn_}) {
        btn->setFixedHeight(30);
        btn->setCursor(Qt::PointingHandCursor);
        dal->addWidget(btn);
    }
    sl->addWidget(dbActions);

    buildSideTree();

    connect(refreshBtn_,  &QPushButton::clicked, this, &MainWindow::refreshSidebar);
    connect(newDbBtn_,    &QPushButton::clicked, this, &MainWindow::createDatabase);
    connect(switchDbBtn_, &QPushButton::clicked, this, &MainWindow::switchDatabase);
    connect(dropDbBtn_,   &QPushButton::clicked, this, &MainWindow::dropDatabase);
    connect(sideTree_, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onSidebarItemDoubleClicked);
}

void MainWindow::buildSideTree() {
    sideTree_->clear();

    // Databases
    auto* dbRoot = new QTreeWidgetItem(sideTree_);
    dbRoot->setText(0, "🗄️  Databases");
    dbRoot->setExpanded(true);

    auto dbs = db_->ListDatabases();
    for (auto& db : dbs) {
        auto* item = new QTreeWidgetItem(dbRoot);
        bool cur = (db == db_->CurrentDatabase());
        item->setText(0, (cur ? "▶ " : "   ") + QString::fromStdString(db));
        item->setData(0, Qt::UserRole, "db");
        item->setData(0, Qt::UserRole+1, QString::fromStdString(db));
        if (cur) {
            QFont f = item->font(0); f.setBold(true);
            item->setFont(0, f);
        }
    }

    // Tables (current db)
    auto* tblRoot = new QTreeWidgetItem(sideTree_);
    tblRoot->setText(0, "📋  Tables");
    tblRoot->setExpanded(true);

    auto tables = db_->ListTables();
    for (auto& tbl : tables) {
        auto* item = new QTreeWidgetItem(tblRoot);
        item->setText(0, "   📄 " + QString::fromStdString(tbl));
        item->setData(0, Qt::UserRole, "table");
        item->setData(0, Qt::UserRole+1, QString::fromStdString(tbl));

        // Columns as children
        auto cols = db_->ListColumns(tbl);
        for (auto& col : cols) {
            auto* colItem = new QTreeWidgetItem(item);
            colItem->setText(0, "      • " + QString::fromStdString(col));
        }
    }
}

void MainWindow::setupSQLMode() {
    sqlModeWidget_ = new QWidget;
    auto* layout = new QVBoxLayout(sqlModeWidget_);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // SQL editor panel
    auto* editorPanel = new QWidget;
    editorPanel->setObjectName("editorPanel");
    auto* epl = new QVBoxLayout(editorPanel);
    epl->setContentsMargins(10, 10, 10, 10);
    epl->setSpacing(8);

    // Top row: label + buttons
    auto* topRow = new QHBoxLayout;
    auto* editorLabel = new QLabel("SQL Editor");
    editorLabel->setObjectName("panelLabel");
    topRow->addWidget(editorLabel);

    auto* hintLabel = new QLabel("Ctrl+Enter or F5 to run");
    hintLabel->setStyleSheet("font-size: 10px; opacity: 0.6;");
    topRow->addWidget(hintLabel);
    topRow->addStretch();

    runBtn_ = new QPushButton("▶   Run");
    runBtn_->setObjectName("runButton");
    runBtn_->setFixedSize(90, 34);
    runBtn_->setCursor(Qt::PointingHandCursor);

    clearBtn_ = new QPushButton("✖  Clear");
    clearBtn_->setObjectName("clearButton");
    clearBtn_->setFixedSize(80, 34);
    clearBtn_->setCursor(Qt::PointingHandCursor);

    topRow->addWidget(clearBtn_);
    topRow->addWidget(runBtn_);
    epl->addLayout(topRow);

    sqlEditor_ = new SqlEditor;
    sqlEditor_->setMinimumHeight(120);
    sqlEditor_->setMaximumHeight(200);
    epl->addWidget(sqlEditor_);

    layout->addWidget(editorPanel);

    // Results + history
    auto* bottomSplitter = new QSplitter(Qt::Horizontal);

    // Results
    auto* resultsPanel = new QWidget;
    resultsPanel->setObjectName("resultsPanel");
    auto* rpl = new QVBoxLayout(resultsPanel);
    rpl->setContentsMargins(0, 0, 0, 0);
    rpl->setSpacing(0);

    auto* resultsHeader = new QLabel("  📊  Query Results");
    resultsHeader->setObjectName("panelHeader");
    resultsHeader->setFixedHeight(32);
    rpl->addWidget(resultsHeader);

    resultTable_ = new ResultTable;
    rpl->addWidget(resultTable_);
    bottomSplitter->addWidget(resultsPanel);

    // History
    auto* histPanel = new QWidget;
    histPanel->setObjectName("histPanel");
    histPanel->setMaximumWidth(250);
    auto* hpl = new QVBoxLayout(histPanel);
    hpl->setContentsMargins(0, 0, 0, 0);
    hpl->setSpacing(0);

    auto* histHeader = new QLabel("  ⏱  Query History");
    histHeader->setObjectName("panelHeader");
    histHeader->setFixedHeight(32);
    hpl->addWidget(histHeader);

    historyList_ = new QListWidget;
    historyList_->setObjectName("historyList");
    historyList_->setToolTip("Double-click to reuse");
    hpl->addWidget(historyList_);

    auto* clearHistBtn = new QPushButton("Clear History");
    clearHistBtn->setFixedHeight(28);
    clearHistBtn->setObjectName("clearButton");
    hpl->addWidget(clearHistBtn);

    bottomSplitter->addWidget(histPanel);
    bottomSplitter->setStretchFactor(0, 4);
    bottomSplitter->setStretchFactor(1, 1);
    layout->addWidget(bottomSplitter);

    // Connections
    connect(runBtn_,   &QPushButton::clicked, this, &MainWindow::runQuery);
    connect(clearBtn_, &QPushButton::clicked, this, &MainWindow::clearEditor);
    connect(sqlEditor_, &SqlEditor::runRequested, this, &MainWindow::runQuery);
    connect(historyList_, &QListWidget::itemDoubleClicked,
            this, &MainWindow::onHistoryDoubleClicked);
    connect(clearHistBtn, &QPushButton::clicked, historyList_, &QListWidget::clear);
}

void MainWindow::setupBeginnerMode() {
    beginnerWidget_ = new QWidget;
    auto* layout = new QVBoxLayout(beginnerWidget_);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // Header
    auto* hdr = new QLabel("🟢  Beginner Mode — No SQL Knowledge Required!");
    hdr->setObjectName("beginnerHeader");
    hdr->setAlignment(Qt::AlignCenter);
    hdr->setFixedHeight(36);
    layout->addWidget(hdr);

    auto* mainSplit = new QSplitter(Qt::Horizontal);

    // Left: action buttons
    auto* actionPanel = new QWidget;
    actionPanel->setObjectName("actionPanel");
    actionPanel->setMaximumWidth(220);
    auto* apl = new QVBoxLayout(actionPanel);
    apl->setContentsMargins(10, 10, 10, 10);
    apl->setSpacing(8);

    auto* actLabel = new QLabel("Choose Action:");
    actLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    apl->addWidget(actLabel);

    struct BtnDef { QString icon; QString text; int idx; };
    QList<BtnDef> btns = {
        {"📋", "Create Table",   0},
        {"➕", "Insert Record",  1},
        {"👁", "View Table",     2},
        {"🗑", "Delete Record",  3},
        {"⚙", "Alter Table",    4},
        {"❌", "Drop Table",     5},
    };

    beginnerStack_ = new QStackedWidget;

    auto* btnGroup = new QButtonGroup(this);
    for (auto& b : btns) {
        auto* btn = new QPushButton(b.icon + "  " + b.text);
        btn->setObjectName("beginnerActionBtn");
        btn->setFixedHeight(38);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btnGroup->addButton(btn, b.idx);
        apl->addWidget(btn);
    }
    btnGroup->buttons().first()->setChecked(true);
    apl->addStretch();

    // Table selector
    auto* tblRow = new QHBoxLayout;
    auto* tblLabel = new QLabel("Table:");
    tblLabel->setStyleSheet("font-weight: bold;");
    beginnerTableCombo_ = new QComboBox;
    beginnerTableCombo_->setObjectName("beginnerCombo");
    auto* refreshTblBtn = new QPushButton("↻");
    refreshTblBtn->setFixedSize(28, 28);
    tblRow->addWidget(tblLabel);
    tblRow->addWidget(beginnerTableCombo_, 1);
    tblRow->addWidget(refreshTblBtn);
    apl->addLayout(tblRow);

    mainSplit->addWidget(actionPanel);

    // Right: stacked forms + output
    auto* rightWidget = new QWidget;
    auto* rLayout = new QVBoxLayout(rightWidget);
    rLayout->setContentsMargins(0, 0, 0, 0);
    rLayout->setSpacing(8);

    // Stacked forms
    beginnerStack_->addWidget(makeBeginnerCreateTable());
    beginnerStack_->addWidget(makeBeginnerInsert());
    beginnerStack_->addWidget(makeBeginnerView());
    beginnerStack_->addWidget(makeBeginnerDelete());
    beginnerStack_->addWidget(makeBeginnerAlter());
    beginnerStack_->addWidget(makeBeginnerDrop());
    rLayout->addWidget(beginnerStack_);

    // Output
    auto* outHeader = new QLabel("  📊  Output");
    outHeader->setObjectName("panelHeader");
    outHeader->setFixedHeight(32);
    rLayout->addWidget(outHeader);

    beginnerOutput_ = new QTextEdit;
    beginnerOutput_->setObjectName("beginnerOutput");
    beginnerOutput_->setReadOnly(true);
    beginnerOutput_->setMaximumHeight(200);
    rLayout->addWidget(beginnerOutput_);

    mainSplit->addWidget(rightWidget);
    mainSplit->setStretchFactor(0, 0);
    mainSplit->setStretchFactor(1, 1);
    layout->addWidget(mainSplit);

    // Connections
    connect(btnGroup, &QButtonGroup::idClicked,
            beginnerStack_, &QStackedWidget::setCurrentIndex);
    connect(refreshTblBtn, &QPushButton::clicked,
            this, &MainWindow::beginnerRefreshTables);
    beginnerRefreshTables();
}

QWidget* MainWindow::makeBeginnerCreateTable() {
    auto* w = new QWidget;
    w->setObjectName("beginnerForm");
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);

    auto* title = new QLabel("📋  Create a New Table");
    title->setObjectName("formTitle");
    layout->addWidget(title);

    auto* form = new QFormLayout;
    auto* nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText("e.g. students");
    nameEdit->setObjectName("formInput");
    form->addRow("Table Name:", nameEdit);

    auto* colsEdit = new QTextEdit;
    colsEdit->setPlaceholderText(
        "One column per line:\n"
        "id INT PRIMARY KEY\n"
        "name VARCHAR(50) NOT NULL\n"
        "age INT DEFAULT 0");
    colsEdit->setObjectName("formInput");
    colsEdit->setMaximumHeight(120);
    form->addRow("Columns:", colsEdit);
    layout->addLayout(form);

    auto* btn = new QPushButton("✅  Create Table");
    btn->setObjectName("runButton");
    btn->setFixedHeight(36);
    layout->addWidget(btn);
    layout->addStretch();

    connect(btn, &QPushButton::clicked, [=]() {
        QString name = nameEdit->text().trimmed();
        QString cols = colsEdit->toPlainText().trimmed();
        if (name.isEmpty() || cols.isEmpty()) {
            beginnerOutput_->append("❌ Please fill in table name and columns.");
            return;
        }
        name.replace(' ', '_');
        QStringList colList = cols.split('\n', Qt::SkipEmptyParts);
        QString sql = "CREATE TABLE " + name + " (" + colList.join(", ") + ");";
        auto result = db_->Execute(sql.toStdString());
        beginnerOutput_->append(result.success ?
            "✅ " + QString::fromStdString(result.message) :
            "❌ " + QString::fromStdString(result.message));
        beginnerRefreshTables();
        refreshSidebar();
    });

    return w;
}

QWidget* MainWindow::makeBeginnerInsert() {
    auto* w = new QWidget;
    w->setObjectName("beginnerForm");
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);

    auto* title = new QLabel("➕  Insert a Record");
    title->setObjectName("formTitle");
    layout->addWidget(title);

    auto* hint = new QLabel("Select a table first, then enter values separated by commas.");
    hint->setWordWrap(true);
    hint->setStyleSheet("color: gray; font-size: 11px;");
    layout->addWidget(hint);

    auto* form = new QFormLayout;
    auto* valuesEdit = new QLineEdit;
    valuesEdit->setPlaceholderText("e.g. 1, 'Salvatore', 21");
    valuesEdit->setObjectName("formInput");
    form->addRow("Values:", valuesEdit);
    layout->addLayout(form);

    auto* btn = new QPushButton("✅  Insert Record");
    btn->setObjectName("runButton");
    btn->setFixedHeight(36);
    layout->addWidget(btn);
    layout->addStretch();

    connect(btn, &QPushButton::clicked, [=]() {
        QString tbl = beginnerTableCombo_->currentText();
        QString vals = valuesEdit->text().trimmed();
        if (tbl.isEmpty() || vals.isEmpty()) {
            beginnerOutput_->append("❌ Select a table and enter values.");
            return;
        }
        QString sql = "INSERT INTO " + tbl + " VALUES (" + vals + ");";
        auto result = db_->Execute(sql.toStdString());
        beginnerOutput_->append(result.success ?
            "✅ " + QString::fromStdString(result.message) :
            "❌ " + QString::fromStdString(result.message));
    });

    return w;
}

QWidget* MainWindow::makeBeginnerView() {
    auto* w = new QWidget;
    w->setObjectName("beginnerForm");
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);

    auto* title = new QLabel("👁  View Table Records");
    title->setObjectName("formTitle");
    layout->addWidget(title);

    auto* form = new QFormLayout;
    auto* filterEdit = new QLineEdit;
    filterEdit->setPlaceholderText("Optional: e.g. age > 20");
    filterEdit->setObjectName("formInput");
    form->addRow("Filter (WHERE):", filterEdit);

    auto* limitSpin = new QSpinBox;
    limitSpin->setRange(0, 10000);
    limitSpin->setValue(0);
    limitSpin->setSpecialValueText("No limit");
    limitSpin->setObjectName("formInput");
    form->addRow("Limit rows:", limitSpin);
    layout->addLayout(form);

    auto* btn = new QPushButton("✅  View Records");
    btn->setObjectName("runButton");
    btn->setFixedHeight(36);
    layout->addWidget(btn);
    layout->addStretch();

    connect(btn, &QPushButton::clicked, [=]() {
        QString tbl = beginnerTableCombo_->currentText();
        if (tbl.isEmpty()) {
            beginnerOutput_->append("❌ Select a table first.");
            return;
        }
        QString sql = "SELECT * FROM " + tbl;
        if (!filterEdit->text().isEmpty())
            sql += " WHERE " + filterEdit->text();
        if (limitSpin->value() > 0)
            sql += " LIMIT " + QString::number(limitSpin->value());
        sql += ";";

        // Switch to SQL mode and run
        sqlEditor_->setPlainText(sql);
        tabWidget_->setCurrentIndex(0);
        runQuery();
    });

    return w;
}

QWidget* MainWindow::makeBeginnerDelete() {
    auto* w = new QWidget;
    w->setObjectName("beginnerForm");
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);

    auto* title = new QLabel("🗑  Delete a Record");
    title->setObjectName("formTitle");
    layout->addWidget(title);

    auto* form = new QFormLayout;
    auto* condEdit = new QLineEdit;
    condEdit->setPlaceholderText("e.g. id = 1");
    condEdit->setObjectName("formInput");
    form->addRow("Where condition:", condEdit);
    layout->addLayout(form);

    auto* btn = new QPushButton("✅  Delete Record");
    btn->setObjectName("dangerButton");
    btn->setFixedHeight(36);
    layout->addWidget(btn);
    layout->addStretch();

    connect(btn, &QPushButton::clicked, [=]() {
        QString tbl = beginnerTableCombo_->currentText();
        QString cond = condEdit->text().trimmed();
        if (tbl.isEmpty() || cond.isEmpty()) {
            beginnerOutput_->append("❌ Select a table and enter a condition.");
            return;
        }
        auto reply = QMessageBox::question(this, "Confirm Delete",
            "Delete from " + tbl + " WHERE " + cond + "?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;

        QString sql = "DELETE FROM " + tbl + " WHERE " + cond + ";";
        auto result = db_->Execute(sql.toStdString());
        beginnerOutput_->append(result.success ?
            "✅ " + QString::fromStdString(result.message) :
            "❌ " + QString::fromStdString(result.message));
    });

    return w;
}

QWidget* MainWindow::makeBeginnerAlter() {
    auto* w = new QWidget;
    w->setObjectName("beginnerForm");
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);

    auto* title = new QLabel("⚙  Alter Table (Add/Drop Column)");
    title->setObjectName("formTitle");
    layout->addWidget(title);

    auto* form = new QFormLayout;
    auto* actionCombo = new QComboBox;
    actionCombo->addItems({"ADD COLUMN", "DROP COLUMN"});
    actionCombo->setObjectName("formInput");
    form->addRow("Action:", actionCombo);

    auto* colNameEdit = new QLineEdit;
    colNameEdit->setPlaceholderText("e.g. email");
    colNameEdit->setObjectName("formInput");
    form->addRow("Column Name:", colNameEdit);

    auto* colTypeEdit = new QLineEdit;
    colTypeEdit->setPlaceholderText("e.g. VARCHAR(100)  (for ADD only)");
    colTypeEdit->setObjectName("formInput");
    form->addRow("Column Type:", colTypeEdit);
    layout->addLayout(form);

    auto* btn = new QPushButton("✅  Apply Change");
    btn->setObjectName("runButton");
    btn->setFixedHeight(36);
    layout->addWidget(btn);
    layout->addStretch();

    connect(btn, &QPushButton::clicked, [=]() {
        QString tbl = beginnerTableCombo_->currentText();
        QString action = actionCombo->currentText();
        QString colName = colNameEdit->text().trimmed();
        if (tbl.isEmpty() || colName.isEmpty()) {
            beginnerOutput_->append("❌ Select table and column name.");
            return;
        }
        QString sql = "ALTER TABLE " + tbl + " " + action + " " + colName;
        if (action == "ADD COLUMN" && !colTypeEdit->text().isEmpty())
            sql += " " + colTypeEdit->text().trimmed();
        sql += ";";
        auto result = db_->Execute(sql.toStdString());
        beginnerOutput_->append(result.success ?
            "✅ " + QString::fromStdString(result.message) :
            "❌ " + QString::fromStdString(result.message));
        refreshSidebar();
    });

    return w;
}

QWidget* MainWindow::makeBeginnerDrop() {
    auto* w = new QWidget;
    w->setObjectName("beginnerForm");
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);

    auto* title = new QLabel("❌  Drop Table");
    title->setObjectName("formTitle");
    layout->addWidget(title);

    auto* warn = new QLabel("⚠️  Warning: This permanently deletes the table and all its data!");
    warn->setWordWrap(true);
    warn->setStyleSheet("color: #cc0000; font-weight: bold;");
    layout->addWidget(warn);

    auto* btn = new QPushButton("❌  Drop Selected Table");
    btn->setObjectName("dangerButton");
    btn->setFixedHeight(36);
    layout->addWidget(btn);
    layout->addStretch();

    connect(btn, &QPushButton::clicked, [=]() {
        QString tbl = beginnerTableCombo_->currentText();
        if (tbl.isEmpty()) {
            beginnerOutput_->append("❌ Select a table first.");
            return;
        }
        auto reply = QMessageBox::question(this, "Confirm Drop",
            "Permanently drop table '" + tbl + "'?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;

        QString sql = "DROP TABLE " + tbl + ";";
        auto result = db_->Execute(sql.toStdString());
        beginnerOutput_->append(result.success ?
            "✅ " + QString::fromStdString(result.message) :
            "❌ " + QString::fromStdString(result.message));
        beginnerRefreshTables();
        refreshSidebar();
    });

    return w;
}

void MainWindow::setupServerMode() {
    serverWidget_ = new QWidget;
    auto* layout = new QVBoxLayout(serverWidget_);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    // Header
    auto* hdr = new QLabel("🌐  TCP Network Server");
    hdr->setObjectName("beginnerHeader");
    hdr->setAlignment(Qt::AlignCenter);
    hdr->setFixedHeight(40);
    layout->addWidget(hdr);

    // Info cards
    auto* infoWidget = new QWidget;
    infoWidget->setObjectName("infoCard");
    auto* infoLayout = new QHBoxLayout(infoWidget);
    infoLayout->setSpacing(16);

    auto makeCard = [](const QString& icon, const QString& title, const QString& value) {
        auto* card = new QWidget;
        card->setObjectName("statCard");
        auto* cl = new QVBoxLayout(card);
        cl->setAlignment(Qt::AlignCenter);
        auto* iconLabel = new QLabel(icon);
        iconLabel->setStyleSheet("font-size: 28px;");
        iconLabel->setAlignment(Qt::AlignCenter);
        auto* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-size: 11px; color: gray;");
        titleLabel->setAlignment(Qt::AlignCenter);
        auto* valueLabel = new QLabel(value);
        valueLabel->setObjectName("statValue");
        valueLabel->setAlignment(Qt::AlignCenter);
        cl->addWidget(iconLabel);
        cl->addWidget(titleLabel);
        cl->addWidget(valueLabel);
        return card;
    };

    infoLayout->addWidget(makeCard("🔌", "PORT", "5555"));
    infoLayout->addWidget(makeCard("💻", "HOST", "localhost"));
    infoLayout->addWidget(makeCard("📡", "PROTOCOL", "TCP"));
    infoLayout->addWidget(makeCard("🔒", "AUTH", "None"));
    layout->addWidget(infoWidget);

    // Controls
    auto* controlRow = new QHBoxLayout;
    serverToggleBtn_ = new QPushButton("▶  Start Server");
    serverToggleBtn_->setObjectName("runButton");
    serverToggleBtn_->setFixedSize(160, 44);
    serverToggleBtn_->setCursor(Qt::PointingHandCursor);

    serverStatusLabel_ = new QLabel("⭕  Server is stopped");
    serverStatusLabel_->setStyleSheet("font-size: 14px; font-weight: bold;");

    controlRow->addWidget(serverToggleBtn_);
    controlRow->addSpacing(20);
    controlRow->addWidget(serverStatusLabel_);
    controlRow->addStretch();
    layout->addLayout(controlRow);

    // How to connect
    auto* howToConnect = new QGroupBox("📖  How to Connect");
    howToConnect->setObjectName("serverGroup");
    auto* htcl = new QVBoxLayout(howToConnect);

    auto addCode = [&](const QString& label, const QString& code) {
        htcl->addWidget(new QLabel("<b>" + label + ":</b>"));
        auto* codeLabel = new QLabel(code);
        codeLabel->setObjectName("codeLabel");
        codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        htcl->addWidget(codeLabel);
        htcl->addSpacing(8);
    };

    addCode("Telnet (Windows CMD)", "telnet localhost 5555");
    addCode("Python",
        "import socket\n"
        "s = socket.socket()\n"
        "s.connect(('localhost', 5555))\n"
        "s.send(b'SELECT * FROM students;')\n"
        "print(s.recv(4096).decode())");
    layout->addWidget(howToConnect);

    // Log
    auto* logHeader = new QLabel("  📝  Server Log");
    logHeader->setObjectName("panelHeader");
    logHeader->setFixedHeight(32);
    layout->addWidget(logHeader);

    serverLog_ = new QTextEdit;
    serverLog_->setObjectName("serverLog");
    serverLog_->setReadOnly(true);
    serverLog_->setPlaceholderText("Server log will appear here...");
    layout->addWidget(serverLog_);

    connect(serverToggleBtn_, &QPushButton::clicked, this, &MainWindow::toggleServer);
}

// ─── Slot Implementations ───────────────────────────────────────────────────

void MainWindow::runQuery() {
    QString sql = sqlEditor_->toPlainText().trimmed();
    if (sql.isEmpty()) return;

    addToHistory(sql.length() > 70 ? sql.left(67) + "..." : sql);

    auto result = db_->Execute(sql.toStdString());
    resultTable_->showResult(result);

    QString status = result.success ?
        "✅  " + QString::fromStdString(result.message) :
        "❌  " + QString::fromStdString(result.message);
    if (result.elapsed_ms > 0)
        status += QString("  •  ⏱ %1 ms").arg(result.elapsed_ms, 0, 'f', 2);
    status += "  •  " + QString::fromStdString(db_->CurrentDatabase());
    statusBar()->showMessage(status);

    QString upper = sql.toUpper();
    if (upper.contains("CREATE") || upper.contains("DROP") ||
        upper.contains("ALTER") || upper.contains("DATABASE")) {
        refreshSidebar();
        beginnerRefreshTables();
        updateDbLabel();
    }
}

void MainWindow::clearEditor() {
    sqlEditor_->clear();
    resultTable_->clear();
}

void MainWindow::toggleTheme() {
    darkMode_ = !darkMode_;
    applyTheme();
    themeBtn_->setText(darkMode_ ? "Light Mode" : "Dark Mode");
}

void MainWindow::applyTheme() {
    QFile f(darkMode_ ? ":/styles/dark.qss" : ":/styles/light.qss");
    if (f.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(f.readAll());
        f.close();
    }
}

void MainWindow::updateDbLabel() {
    dbLabel_->setText("🗄  Database: <b>" +
        QString::fromStdString(db_->CurrentDatabase()) + "</b>");
    dbLabel_->setTextFormat(Qt::RichText);
    dbLabel_->setStyleSheet("color: rgba(255,255,255,0.95); font-size: 13px;");
}

void MainWindow::refreshSidebar() {
    buildSideTree();
}

void MainWindow::onSidebarItemDoubleClicked(QTreeWidgetItem* item, int) {
    QString type = item->data(0, Qt::UserRole).toString();
    QString name = item->data(0, Qt::UserRole+1).toString();

    if (type == "db") {
        db_->UseDatabase(name.toStdString());
        updateDbLabel();
        refreshSidebar();
        beginnerRefreshTables();
        statusBar()->showMessage("✅  Switched to database: " + name);
    } else if (type == "table") {
        sqlEditor_->setPlainText("SELECT * FROM " + name + ";");
        tabWidget_->setCurrentIndex(0);
        runQuery();
    }
}

void MainWindow::addToHistory(const QString& sql) {
    for (int i = 0; i < historyList_->count(); i++)
        if (historyList_->item(i)->text() == sql) return;
    historyList_->insertItem(0, sql);
    if (historyList_->count() > 100)
        delete historyList_->takeItem(historyList_->count() - 1);
}

void MainWindow::onHistoryDoubleClicked(QListWidgetItem* item) {
    sqlEditor_->setPlainText(item->text());
    tabWidget_->setCurrentIndex(0);
}

void MainWindow::beginnerRefreshTables() {
    beginnerTableCombo_->clear();
    auto tables = db_->ListTables();
    for (auto& t : tables)
        beginnerTableCombo_->addItem(QString::fromStdString(t));
}

void MainWindow::toggleServer() {
    serverRunning_ = !serverRunning_;
    if (serverRunning_) {
        serverToggleBtn_->setText("⬛  Stop Server");
        serverToggleBtn_->setObjectName("dangerButton");
        serverStatusLabel_->setText("🟢  Server running on port 5555");
        serverStatusLabel_->setStyleSheet("color: #2ea82e; font-size: 14px; font-weight: bold;");
        logServer("✅ Server started on localhost:5555");
        logServer("⏳ Waiting for connections...");
        logServer("💡 Connect using: telnet localhost 5555");
    } else {
        serverToggleBtn_->setText("▶  Start Server");
        serverToggleBtn_->setObjectName("runButton");
        serverStatusLabel_->setText("⭕  Server is stopped");
        serverStatusLabel_->setStyleSheet("font-size: 14px; font-weight: bold;");
        logServer("⭕ Server stopped.");
    }
    applyTheme(); // reapply for button style change
}

void MainWindow::logServer(const QString& msg) {
    serverLog_->append(QDateTime::currentDateTime().toString("[hh:mm:ss]  ") + msg);
}

void MainWindow::createDatabase() {
    bool ok;
    QString name = QInputDialog::getText(this, "Create Database",
        "Enter database name:", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        db_->CreateDatabase(name.toStdString());
        updateDbLabel();
        refreshSidebar();
        statusBar()->showMessage("✅  Database '" + name + "' created and selected.");
    }
}

void MainWindow::switchDatabase() {
    auto dbs = db_->ListDatabases();
    QStringList list;
    for (auto& d : dbs) list << QString::fromStdString(d);
    bool ok;
    QString name = QInputDialog::getItem(this, "Switch Database",
        "Select database:", list, 0, false, &ok);
    if (ok && !name.isEmpty()) {
        db_->UseDatabase(name.toStdString());
        updateDbLabel();
        refreshSidebar();
        beginnerRefreshTables();
        statusBar()->showMessage("✅  Switched to: " + name);
    }
}

void MainWindow::dropDatabase() {
    auto dbs = db_->ListDatabases();
    QStringList list;
    for (auto& d : dbs) {
        if (d != db_->CurrentDatabase())
            list << QString::fromStdString(d);
    }
    if (list.isEmpty()) {
        QMessageBox::information(this, "Drop Database",
            "No other databases to drop.");
        return;
    }
    bool ok;
    QString name = QInputDialog::getItem(this, "Drop Database",
        "Select database to drop:", list, 0, false, &ok);
    if (ok && !name.isEmpty()) {
        auto reply = QMessageBox::question(this, "Confirm",
            "Drop database '" + name + "'? This cannot be undone.",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            db_->DropDatabase(name.toStdString());
            refreshSidebar();
            statusBar()->showMessage("✅  Database '" + name + "' dropped.");
        }
    }
}

void MainWindow::switchTab(int index) {
    tabWidget_->setCurrentIndex(index);
}

QString MainWindow::currentTable() {
    return beginnerTableCombo_->currentText();
}

void MainWindow::beginnerCreateTable() {}
void MainWindow::beginnerInsertRecord() {}
void MainWindow::beginnerViewTable() {}
void MainWindow::beginnerDeleteRecord() {}
void MainWindow::beginnerDropTable() {}
void MainWindow::beginnerAddColumn() {}
