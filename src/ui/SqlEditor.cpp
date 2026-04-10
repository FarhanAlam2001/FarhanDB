#include "ui/SqlEditor.h"

SqlEditor::SqlEditor(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setObjectName("sqlEditor");
    setPlaceholderText(
        "Type SQL here...\n"
        "Examples:\n"
        "  SELECT * FROM students;\n"
        "  CREATE TABLE students (id INT PRIMARY KEY, name VARCHAR(50));\n"
        "  INSERT INTO students VALUES (1, 'Salvatore');\n\n"
        "Press Ctrl+Enter or F5 to run."
    );
    setMinimumHeight(100);
    setMaximumHeight(180);
    setLineWrapMode(QPlainTextEdit::NoWrap);
}

void SqlEditor::keyPressEvent(QKeyEvent* event) {
    // Ctrl+Enter or F5 → run query
    if ((event->key() == Qt::Key_Return &&
         event->modifiers() == Qt::ControlModifier) ||
         event->key() == Qt::Key_F5) {
        emit runRequested();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}
