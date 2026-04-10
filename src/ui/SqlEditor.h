#pragma once
#include <QPlainTextEdit>
#include <QKeyEvent>

class SqlEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit SqlEditor(QWidget* parent = nullptr);

signals:
    void runRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
};
