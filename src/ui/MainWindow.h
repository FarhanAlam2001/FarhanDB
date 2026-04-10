#pragma once
#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QListWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QGroupBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QTextEdit>
#include <QProgressBar>
#include "bridge/DBBridge.h"
#include "ui/SqlEditor.h"
#include "ui/ResultTable.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void runQuery();
    void clearEditor();
    void toggleTheme();
    void switchTab(int index);
    void refreshSidebar();
    void onSidebarItemDoubleClicked(QTreeWidgetItem* item, int col);
    void addToHistory(const QString& sql);
    void onHistoryDoubleClicked(QListWidgetItem* item);

    // Beginner mode
    void beginnerCreateTable();
    void beginnerInsertRecord();
    void beginnerViewTable();
    void beginnerDeleteRecord();
    void beginnerDropTable();
    void beginnerAddColumn();
    void beginnerRefreshTables();

    // Server mode
    void toggleServer();

    // Database management
    void createDatabase();
    void switchDatabase();
    void dropDatabase();

private:
    FarhanDB::DBBridge* db_;
    bool darkMode_ = false;
    bool serverRunning_ = false;

    // Layout
    QWidget*        topBar_;
    QLabel*         titleLabel_;
    QLabel*         dbLabel_;
    QPushButton*    themeBtn_;
    QTabWidget*     tabWidget_;
    QSplitter*      mainSplitter_;

    // Sidebar
    QWidget*        sidebar_;
    QTreeWidget*    sideTree_;
    QPushButton*    newDbBtn_;
    QPushButton*    switchDbBtn_;
    QPushButton*    dropDbBtn_;
    QPushButton*    refreshBtn_;

    // SQL Mode
    QWidget*        sqlModeWidget_;
    SqlEditor*      sqlEditor_;
    ResultTable*    resultTable_;
    QListWidget*    historyList_;
    QPushButton*    runBtn_;
    QPushButton*    clearBtn_;
    QLabel*         sqlStatusLabel_;

    // Beginner Mode
    QWidget*        beginnerWidget_;
    QComboBox*      beginnerTableCombo_;
    QTextEdit*      beginnerOutput_;
    QWidget*        beginnerFormArea_;
    QStackedWidget* beginnerStack_;

    // Server Mode
    QWidget*        serverWidget_;
    QPushButton*    serverToggleBtn_;
    QLabel*         serverStatusLabel_;
    QTextEdit*      serverLog_;
    QLabel*         serverInfoLabel_;

    void setupUI();
    void setupTopBar();
    void setupSidebar();
    void setupSQLMode();
    void setupBeginnerMode();
    void setupServerMode();
    void applyTheme();
    void updateDbLabel();
    void buildSideTree();
    void logServer(const QString& msg);
    QString currentTable();

    // Beginner helpers
    QWidget* makeBeginnerCreateTable();
    QWidget* makeBeginnerInsert();
    QWidget* makeBeginnerView();
    QWidget* makeBeginnerDelete();
    QWidget* makeBeginnerDrop();
    QWidget* makeBeginnerAlter();
};
