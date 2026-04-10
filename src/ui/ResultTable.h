#pragma once
#include <QTableWidget>
#include <QLabel>
#include <QWidget>
#include <QVBoxLayout>
#include "bridge/DBBridge.h"

class ResultTable : public QWidget {
    Q_OBJECT
public:
    explicit ResultTable(QWidget* parent = nullptr);
    void showResult(const FarhanDB::QueryResult& result);
    void showMessage(const QString& msg, bool success = true);
    void clear();

private:
    QTableWidget* table_;
    QLabel*       statusLabel_;
};
