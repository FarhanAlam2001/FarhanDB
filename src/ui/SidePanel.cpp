#include "ui/SidePanel.h"
#include <QInputDialog>
#include <QMessageBox>

SidePanel::SidePanel(FarhanDB::DBBridge* db, QWidget* parent)
    : QWidget(parent), db_(db)
{
    setObjectName("sidePanel");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto* header = new QWidget;
    header->setFixedHeight(36);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(8, 4, 8, 4);

    dbLabel_ = new QLabel("🗄 " + QString::fromStdString(db_->CurrentDatabase()));
    dbLabel_->setObjectName("sideLabel");
    dbLabel_->setStyleSheet("font-weight: bold; font-size: 13px;");

    newDbBtn_ = new QPushButton("+");
    newDbBtn_->setObjectName("newDbBtn");
    newDbBtn_->setFixedSize(24, 24);
    newDbBtn_->setToolTip("New Database");

    hl->addWidget(dbLabel_);
    hl->addStretch();
    hl->addWidget(newDbBtn_);
    layout->addWidget(header);

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    layout->addWidget(sep);

    // Tree
    tree_ = new QTreeWidget;
    tree_->setObjectName("sideTree");
    tree_->setHeaderHidden(true);
    tree_->setAnimated(true);
    tree_->setIndentation(16);
    layout->addWidget(tree_);

    connect(newDbBtn_, &QPushButton::clicked, [this]() {
        bool ok;
        QString name = QInputDialog::getText(this, "New Database",
            "Database name:", QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            db_->CreateDatabase(name.toStdString());
            refresh();
            emit newDatabaseRequested();
        }
    });

    connect(tree_, &QTreeWidget::itemDoubleClicked,
            this, &SidePanel::onItemDoubleClicked);

    buildTree();
}

void SidePanel::buildTree() {
    tree_->clear();

    // Databases section
    auto* dbSection = new QTreeWidgetItem(tree_);
    dbSection->setText(0, "📁 Databases");
    dbSection->setExpanded(true);

    auto dbs = db_->ListDatabases();
    for (auto& db : dbs) {
        auto* dbItem = new QTreeWidgetItem(dbSection);
        bool isCurrent = (db == db_->CurrentDatabase());
        dbItem->setText(0, (isCurrent ? "● " : "○ ") +
                           QString::fromStdString(db));
        dbItem->setData(0, Qt::UserRole, "database");
        dbItem->setData(0, Qt::UserRole + 1, QString::fromStdString(db));
        if (isCurrent) {
            QFont f = dbItem->font(0);
            f.setBold(true);
            dbItem->setFont(0, f);
        }
    }

    // Tables section (current DB)
    auto* tblSection = new QTreeWidgetItem(tree_);
    tblSection->setText(0, "📋 Tables");
    tblSection->setExpanded(true);

    // Get tables from catalog by running SHOW TABLES equivalent
    // We query via Execute and parse result
    auto result = db_->Execute("SELECT * FROM __tables__;");
    // Since we don't have SHOW TABLES, we get tables from sidebar via catalog
    // Use a workaround - try to get from catalog directly
    // For now show placeholder - will be populated via refresh
}

void SidePanel::refresh() {
    dbLabel_->setText("🗄 " + QString::fromStdString(db_->CurrentDatabase()));
    buildTree();
}

void SidePanel::onItemDoubleClicked(QTreeWidgetItem* item, int) {
    QString type = item->data(0, Qt::UserRole).toString();
    QString name = item->data(0, Qt::UserRole + 1).toString();

    if (type == "database") {
        db_->UseDatabase(name.toStdString());
        refresh();
        emit switchDatabaseRequested(name);
    } else if (type == "table") {
        emit tableClicked(name);
    }
}
