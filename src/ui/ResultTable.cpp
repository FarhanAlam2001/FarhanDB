#include "ui/ResultTable.h"
#include <QHeaderView>
#include <QColor>

ResultTable::ResultTable(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    table_ = new QTableWidget;
    table_->setObjectName("resultTable");
    table_->setAlternatingRowColors(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->verticalHeader()->setDefaultSectionSize(28);
    table_->verticalHeader()->hide();
    table_->setSortingEnabled(true);
    layout->addWidget(table_);

    statusLabel_ = new QLabel;
    statusLabel_->setObjectName("statusLabel");
    statusLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusLabel_->setFixedHeight(24);
    layout->addWidget(statusLabel_);
}

void ResultTable::showResult(const FarhanDB::QueryResult& result) {
    table_->clear();
    table_->setRowCount(0);
    table_->setColumnCount(0);

    if (!result.success) {
        showMessage("❌ " + QString::fromStdString(result.message), false);
        return;
    }

    if (result.rows.empty()) {
        showMessage("✅ " + QString::fromStdString(result.message));
        return;
    }

    // Set columns
    table_->setColumnCount((int)result.column_names.size());
    QStringList headers;
    for (auto& col : result.column_names)
        headers << QString::fromStdString(col);
    table_->setHorizontalHeaderLabels(headers);

    // Set rows
    table_->setRowCount((int)result.rows.size());
    for (int r = 0; r < (int)result.rows.size(); r++) {
        for (int c = 0; c < (int)result.rows[r].size(); c++) {
            auto* item = new QTableWidgetItem(
                QString::fromStdString(result.rows[r][c]));
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            table_->setItem(r, c, item);
        }
    }

    table_->resizeColumnsToContents();

    // Status
    QString status = QString("✅ %1 row(s) returned").arg(result.rows.size());
    if (result.elapsed_ms > 0)
        status += QString("  •  ⏱ %1 ms").arg(result.elapsed_ms, 0, 'f', 2);
    statusLabel_->setText(status);
    statusLabel_->setStyleSheet("color: #2ea82e; font-weight: bold;");
}

void ResultTable::showMessage(const QString& msg, bool success) {
    table_->clear();
    table_->setRowCount(0);
    table_->setColumnCount(0);
    statusLabel_->setText(msg);
    statusLabel_->setStyleSheet(success ?
        "color: #2ea82e; font-weight: bold;" :
        "color: #cc0000; font-weight: bold;");
}

void ResultTable::clear() {
    table_->clear();
    table_->setRowCount(0);
    table_->setColumnCount(0);
    statusLabel_->clear();
}
