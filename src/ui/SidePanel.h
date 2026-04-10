#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include "bridge/DBBridge.h"

class SidePanel : public QWidget {
    Q_OBJECT
public:
    explicit SidePanel(FarhanDB::DBBridge* db, QWidget* parent = nullptr);
    void refresh();

signals:
    void tableClicked(const QString& tableName);
    void newDatabaseRequested();
    void switchDatabaseRequested(const QString& name);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int col);

private:
    FarhanDB::DBBridge* db_;
    QTreeWidget*        tree_;
    QLabel*             dbLabel_;
    QPushButton*        newDbBtn_;

    void buildTree();
};
