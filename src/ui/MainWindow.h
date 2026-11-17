#pragma once

#include "core/Account.h"

#include <QMainWindow>

class ParkingRepository;
class QAction;
class QLabel;
class QLineEdit;
class QListView;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTimer;
class SlotListModel;
struct Slot;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(ParkingRepository &repository, Account account, QWidget *parent = nullptr);

private:
    void buildUi();
    QWidget *buildHeader(QWidget *parent);
    QWidget *buildSummaryPanel(QWidget *parent);
    QWidget *buildGridSection(QWidget *parent);
    QWidget *buildDetailsPanel(QWidget *parent);

    void reload();
    void updateSummary();
    void updateDetails();
    void applyAccount();
    void applyThemeIcons();
    void selectSlotByName(const QString &name);
    void setManageHint(const QString &text, bool isError);
    [[nodiscard]] const Slot *currentSlot() const;
    [[nodiscard]] QString nextFreeSlotName() const;

    void bookSelected();
    void cancelSelected();
    void activateSelected();
    void addSlot();
    void deleteSelected();
    void newUserAccount();
    void signOut();
    void showError(const QString &message);

    ParkingRepository &m_repository;
    Account m_account;
    SlotListModel *m_model = nullptr;
    QListView *m_grid = nullptr;

    // Header
    QLabel *m_logo = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_userLabel = nullptr;
    QPushButton *m_refreshButton = nullptr;

    // Summary sidebar
    QLabel *m_totalValue = nullptr;
    QLabel *m_freeValue = nullptr;
    QLabel *m_occupiedValue = nullptr;
    QLabel *m_occupancyLabel = nullptr;
    QProgressBar *m_occupancyBar = nullptr;
    QListWidget *m_occupiedList = nullptr;

    // Details and actions
    QLabel *m_slotTitle = nullptr;
    QLabel *m_slotState = nullptr;
    QPushButton *m_bookButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QWidget *m_manageSection = nullptr;      // administrators only
    QLineEdit *m_slotNameEdit = nullptr;
    QPushButton *m_addButton = nullptr;
    QLabel *m_manageHint = nullptr;
    QPushButton *m_deleteButton = nullptr;

    // Account menu
    QAction *m_userAction = nullptr;
    QAction *m_newUserAction = nullptr;

    QTimer *m_refreshTimer = nullptr;
};
