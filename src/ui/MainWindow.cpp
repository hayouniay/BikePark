#include "ui/MainWindow.h"

#include "core/ParkingRepository.h"
#include "ui/AccountDialogs.h"
#include "ui/Icons.h"
#include "ui/SlotDelegate.h"
#include "ui/SlotListModel.h"
#include "ui/Theme.h"

#include <QAction>
#include <QChar>
#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QStringList>
#include <QStyleHints>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace {

constexpr int kRefreshIntervalMs = 15000;

// Re-reads a dynamic property so the stylesheet selectors that depend on it take effect.
void repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QLabel *makeCaption(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("caption"));
    return label;
}

QFrame *makeDivider(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("divider"));
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    return line;
}

// One summary line: coloured dot, name, and value on the right.
QWidget *makeSummaryRow(const QString &dotName, const QString &name, const QString &valueName,
                        QLabel *&valueLabel, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *dot = new QLabel(QStringLiteral("●"), row);
    dot->setObjectName(dotName);
    auto *label = new QLabel(name, row);
    label->setObjectName(QStringLiteral("rowName"));
    valueLabel = new QLabel(QStringLiteral("0"), row);
    valueLabel->setObjectName(valueName);

    layout->addWidget(dot);
    layout->addWidget(label, 1);
    layout->addWidget(valueLabel);
    return row;
}

} // namespace

MainWindow::MainWindow(ParkingRepository &repository, Account account, QWidget *parent)
    : QMainWindow(parent), m_repository(repository), m_account(std::move(account))
{
    buildUi();
    reload();
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("Bike Parking"));
    setWindowIcon(Icons::parking(Theme::current().accent));
    resize(1200, 760);
    setMinimumSize(1040, 640);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("central"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(20, 16, 20, 12);
    root->setSpacing(16);

    root->addWidget(buildHeader(central));

    auto *body = new QHBoxLayout;
    body->setSpacing(16);
    body->addWidget(buildSummaryPanel(central));
    body->addWidget(buildGridSection(central), 1);
    body->addWidget(buildDetailsPanel(central));
    root->addLayout(body, 1);

    setCentralWidget(central);

    // Account menu. The administrator-only entries are shown or hidden in applyAccount().
    auto *accountMenu = menuBar()->addMenu(tr("&Account"));
    m_userAction = accountMenu->addAction(QStringLiteral("-"));
    m_userAction->setEnabled(false);
    accountMenu->addSeparator();
    m_newUserAction = accountMenu->addAction(tr("New user account…"), this, &MainWindow::newUserAccount);
    accountMenu->addAction(tr("Sign out…"), this, &MainWindow::signOut);

    // Shortcuts. Delete is scoped to the grid so typing in the name field can never cancel a booking.
    auto *deleteKey = new QShortcut(QKeySequence::Delete, m_grid);
    deleteKey->setContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteKey, &QShortcut::activated, this, &MainWindow::cancelSelected);
    new QShortcut(QKeySequence::Refresh, this, [this] { reload(); });

    // Wiring.
    connect(m_grid->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this] { updateDetails(); });
    // 'activated' fires on double-click and on Enter/Return.
    connect(m_grid, &QListView::activated, this, &MainWindow::activateSelected);
    connect(m_bookButton, &QPushButton::clicked, this, &MainWindow::bookSelected);
    connect(m_cancelButton, &QPushButton::clicked, this, &MainWindow::cancelSelected);
    connect(m_deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelected);
    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::addSlot);
    connect(m_slotNameEdit, &QLineEdit::returnPressed, this, &MainWindow::addSlot);
    connect(m_slotNameEdit, &QLineEdit::textEdited, this, [this] { setManageHint(QString(), false); });
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::reload);
    connect(m_occupiedList, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { selectSlotByName(item->data(Qt::UserRole).toString()); });

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            &MainWindow::applyThemeIcons);
    applyThemeIcons();
    setManageHint(QString(), false);
    applyAccount();

    // Keep counts current when another window or user changes a slot.
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(kRefreshIntervalMs);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::reload);
    m_refreshTimer->start();
}

QWidget *MainWindow::buildHeader(QWidget *parent)
{
    auto *header = new QFrame(parent);
    header->setObjectName(QStringLiteral("header"));
    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(20, 14, 20, 14);
    layout->setSpacing(14);

    m_logo = new QLabel(header);
    m_logo->setFixedSize(44, 44);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(0);
    auto *title = new QLabel(tr("Bike Parking"), header);
    title->setObjectName(QStringLiteral("title"));
    m_subtitle = new QLabel(header);
    m_subtitle->setObjectName(QStringLiteral("subtitle"));
    titles->addWidget(title);
    titles->addWidget(m_subtitle);

    m_userLabel = new QLabel(header);
    m_userLabel->setObjectName(QStringLiteral("userChip"));

    m_refreshButton = new QPushButton(tr("Refresh"), header);
    m_refreshButton->setObjectName(QStringLiteral("secondary"));
    m_refreshButton->setToolTip(tr("Reload slots and bookings (F5)"));
    m_refreshButton->setCursor(Qt::PointingHandCursor);

    layout->addWidget(m_logo);
    layout->addLayout(titles, 1);
    layout->addWidget(m_userLabel);
    layout->addWidget(m_refreshButton);
    return header;
}

QWidget *MainWindow::buildSummaryPanel(QWidget *parent)
{
    auto *panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("panel"));
    panel->setFixedWidth(260);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    layout->addWidget(makeCaption(tr("SUMMARY"), panel));
    layout->addWidget(makeSummaryRow(QStringLiteral("dotTotal"), tr("Total slots"),
                                     QStringLiteral("valueTotal"), m_totalValue, panel));
    layout->addWidget(makeSummaryRow(QStringLiteral("dotFree"), tr("Free"),
                                     QStringLiteral("valueFree"), m_freeValue, panel));
    layout->addWidget(makeSummaryRow(QStringLiteral("dotOccupied"), tr("Occupied"),
                                     QStringLiteral("valueOccupied"), m_occupiedValue, panel));

    m_occupancyLabel = new QLabel(panel);
    m_occupancyLabel->setObjectName(QStringLiteral("hint"));
    m_occupancyBar = new QProgressBar(panel);
    m_occupancyBar->setObjectName(QStringLiteral("occupancy"));
    m_occupancyBar->setRange(0, 100);
    m_occupancyBar->setTextVisible(false);
    m_occupancyBar->setFixedHeight(10);
    layout->addWidget(m_occupancyLabel);
    layout->addWidget(m_occupancyBar);

    layout->addSpacing(6);
    layout->addWidget(makeDivider(panel));
    layout->addWidget(makeCaption(tr("OCCUPIED SLOTS"), panel));

    m_occupiedList = new QListWidget(panel);
    m_occupiedList->setObjectName(QStringLiteral("occupiedList"));
    m_occupiedList->setFocusPolicy(Qt::NoFocus);
    m_occupiedList->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_occupiedList, 1);
    return panel;
}

QWidget *MainWindow::buildGridSection(QWidget *parent)
{
    auto *section = new QWidget(parent);
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *heading = new QHBoxLayout;
    heading->addWidget(makeCaption(tr("PARKING SLOTS"), section));
    heading->addStretch();
    auto *hint = new QLabel(tr("Enter or double-click a slot to book it · Delete releases your booking"), section);
    hint->setObjectName(QStringLiteral("hint"));
    heading->addWidget(hint);
    layout->addLayout(heading);

    m_model = new SlotListModel(this);
    m_grid = new QListView(section);
    m_grid->setObjectName(QStringLiteral("slotGrid"));
    m_grid->setAccessibleName(tr("Bike parking slots"));
    m_grid->setModel(m_model);
    m_grid->setItemDelegate(new SlotDelegate(m_grid));
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setUniformItemSizes(true);
    m_grid->setSpacing(4);
    m_grid->setSelectionMode(QAbstractItemView::SingleSelection);
    m_grid->setMouseTracking(true);
    m_grid->setFrameShape(QFrame::NoFrame);
    m_grid->viewport()->setAutoFillBackground(false);
    layout->addWidget(m_grid, 1);
    return section;
}

QWidget *MainWindow::buildDetailsPanel(QWidget *parent)
{
    auto *panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("panel"));
    panel->setFixedWidth(290);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    layout->addWidget(makeCaption(tr("SELECTED SLOT"), panel));
    m_slotTitle = new QLabel(QStringLiteral("-"), panel);
    m_slotTitle->setObjectName(QStringLiteral("slotTitle"));
    m_slotState = new QLabel(panel);
    m_slotState->setObjectName(QStringLiteral("slotState"));
    m_slotState->setWordWrap(true);
    layout->addWidget(m_slotTitle);
    layout->addWidget(m_slotState);
    layout->addSpacing(6);

    m_bookButton = new QPushButton(tr("Book this slot"), panel);
    m_bookButton->setObjectName(QStringLiteral("primary"));
    m_bookButton->setToolTip(tr("Book the selected slot (Enter)"));
    m_cancelButton = new QPushButton(tr("Release booking"), panel);
    m_cancelButton->setObjectName(QStringLiteral("secondary"));
    for (QPushButton *button : {m_bookButton, m_cancelButton}) {
        button->setIconSize(QSize(18, 18));
        button->setCursor(Qt::PointingHandCursor);
        layout->addWidget(button);
    }

    layout->addSpacing(8);
    layout->addWidget(makeDivider(panel));

    // Administrators only: adding and deleting slots. Hidden for members in applyAccount().
    m_manageSection = new QWidget(panel);
    auto *manage = new QVBoxLayout(m_manageSection);
    manage->setContentsMargins(0, 0, 0, 0);
    manage->setSpacing(10);
    manage->addWidget(makeCaption(tr("MANAGE SLOTS"), m_manageSection));

    auto *hint = new QLabel(tr("Add a slot by name. Leave the name empty to add the next free letter."), m_manageSection);
    hint->setObjectName(QStringLiteral("hint"));
    hint->setWordWrap(true);
    manage->addWidget(hint);

    auto *addRow = new QHBoxLayout;
    addRow->setSpacing(8);
    m_slotNameEdit = new QLineEdit(m_manageSection);
    m_slotNameEdit->setObjectName(QStringLiteral("slotName"));
    m_slotNameEdit->setMaxLength(6);
    m_slotNameEdit->setAccessibleName(tr("New slot name"));
    m_addButton = new QPushButton(tr("Add"), m_manageSection);
    m_addButton->setObjectName(QStringLiteral("secondary"));
    m_addButton->setIconSize(QSize(18, 18));
    m_addButton->setCursor(Qt::PointingHandCursor);
    addRow->addWidget(m_slotNameEdit, 1);
    addRow->addWidget(m_addButton);
    manage->addLayout(addRow);

    m_manageHint = new QLabel(m_manageSection);
    m_manageHint->setObjectName(QStringLiteral("manageHint"));
    m_manageHint->setWordWrap(true);
    manage->addWidget(m_manageHint);

    m_deleteButton = new QPushButton(tr("Delete selected slot"), m_manageSection);
    m_deleteButton->setObjectName(QStringLiteral("danger"));
    m_deleteButton->setIconSize(QSize(18, 18));
    m_deleteButton->setCursor(Qt::PointingHandCursor);
    m_deleteButton->setToolTip(tr("Remove the selected slot. Only free slots can be deleted."));
    manage->addWidget(m_deleteButton);

    layout->addWidget(m_manageSection);
    layout->addStretch();
    return panel;
}

void MainWindow::applyAccount()
{
    const QString role = m_account.isAdmin ? tr("administrator") : tr("member");
    m_userLabel->setText(tr("%1 · %2").arg(m_account.displayName, role));
    m_userAction->setText(tr("Signed in as %1").arg(m_account.displayName));
    m_newUserAction->setVisible(m_account.isAdmin);
    m_manageSection->setVisible(m_account.isAdmin);
    updateDetails();
}

void MainWindow::applyThemeIcons()
{
    const Theme::Palette &palette = Theme::current();
    setWindowIcon(Icons::parking(palette.accent));
    m_logo->setPixmap(Icons::parking(palette.accent).pixmap(44, 44));
    m_bookButton->setIcon(Icons::book(Qt::white));
    m_cancelButton->setIcon(Icons::release(palette.text));
    m_addButton->setIcon(Icons::add(palette.text));
    m_deleteButton->setIcon(Icons::trash(palette.booked));
    m_refreshButton->setIcon(Icons::refresh(palette.text));
}

void MainWindow::reload()
{
    // Remember the selected slot by name so it survives the model reset.
    const Slot *current = currentSlot();
    const QString previous = current ? current->name : QString();

    // Background refreshes report problems in the status bar only, never as modal dialogs.
    if (const auto expired = m_repository.releaseExpired(QDateTime::currentDateTimeUtc(), kBookingLimitHours);
        !expired)
        statusBar()->showMessage(tr("Could not check expired bookings: %1").arg(expired.error()), 6000);
    else if (*expired > 0)
        statusBar()->showMessage(tr("Released %n expired booking(s).", nullptr, *expired), 5000);

    const auto result = m_repository.allSlots();
    if (!result) {
        statusBar()->showMessage(tr("Could not load slots: %1").arg(result.error()), 6000);
        return;
    }
    m_model->setSlots(*result);

    if (!previous.isEmpty())
        selectSlotByName(previous);

    const QString next = nextFreeSlotName();
    m_slotNameEdit->setPlaceholderText(next.isEmpty() ? tr("Type a name") : tr("e.g. %1").arg(next));

    updateSummary();
    updateDetails();
}

void MainWindow::updateSummary()
{
    const int total = m_model->rowCount();
    const int booked = m_model->bookedCount();
    const int free = total - booked;
    const int percent = total > 0 ? qRound(booked * 100.0 / total) : 0;

    m_totalValue->setText(QString::number(total));
    m_freeValue->setText(QString::number(free));
    m_occupiedValue->setText(QString::number(booked));
    m_occupancyBar->setValue(percent);
    m_occupancyLabel->setText(tr("%1% occupied").arg(percent));

    if (total == 0)
        m_subtitle->setText(tr("No slots yet. An administrator can add one."));
    else if (free == 0)
        m_subtitle->setText(tr("Every slot is taken. Check back later or release one."));
    else
        m_subtitle->setText(tr("%n free slot(s) right now.", nullptr, free));

    m_occupiedList->clear();
    if (booked == 0) {
        auto *empty = new QListWidgetItem(tr("No bookings yet"), m_occupiedList);
        empty->setFlags(Qt::NoItemFlags);
        return;
    }
    for (int row = 0; row < total; ++row) {
        const Slot *slot = m_model->slotAt(row);
        if (!slot || !slot->booked)
            continue;
        const QString who = slot->ownerId == m_account.id ? tr("you")
                            : slot->owner.isEmpty()       ? tr("unknown")
                                                          : slot->owner;
        const QString when = slot->bookedAt.isValid()
                                 ? slot->bookedAt.toLocalTime().toString(QStringLiteral("HH:mm"))
                                 : QString();
        auto *item = new QListWidgetItem(
            when.isEmpty() ? tr("%1   %2").arg(slot->name, who) : tr("%1   %2   %3").arg(slot->name, who, when),
            m_occupiedList);
        item->setData(Qt::UserRole, slot->name);
        item->setToolTip(tr("Click to select slot %1").arg(slot->name));
    }
}

void MainWindow::updateDetails()
{
    const Slot *slot = currentSlot();
    const bool booked = slot && slot->booked;
    const bool free = slot && !slot->booked;
    const bool mine = booked && slot->ownerId == m_account.id;
    const bool canRelease = booked && (mine || m_account.isAdmin);

    m_slotTitle->setText(slot ? tr("Slot %1").arg(slot->name) : tr("No slot selected"));
    if (!slot)
        m_slotState->setText(tr("Pick a slot in the grid to see its status."));
    else if (mine)
        m_slotState->setText(tr("Booked by you"));
    else
        m_slotState->setText(describeSlot(*slot));
    m_slotState->setProperty("state", booked ? QStringLiteral("booked") : QStringLiteral("free"));
    repolish(m_slotState);

    m_bookButton->setEnabled(free);

    m_cancelButton->setEnabled(canRelease);
    m_cancelButton->setText(mine ? tr("Release my booking") : tr("Release booking"));
    m_cancelButton->setToolTip(booked && !canRelease
                                   ? tr("Only the person who booked this slot, or an administrator, can release it.")
                                   : tr("Release the selected booking (Delete)"));

    m_deleteButton->setEnabled(free && m_account.isAdmin);
    m_deleteButton->setText(slot ? tr("Delete slot %1").arg(slot->name) : tr("Delete selected slot"));
}

void MainWindow::selectSlotByName(const QString &name)
{
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const Slot *slot = m_model->slotAt(row);
        if (slot && slot->name == name) {
            m_grid->setCurrentIndex(m_model->index(row, 0));
            m_grid->scrollTo(m_model->index(row, 0));
            return;
        }
    }
}

void MainWindow::setManageHint(const QString &text, bool isError)
{
    m_manageHint->setText(text);
    m_manageHint->setProperty("error", isError);
    m_manageHint->setVisible(!text.isEmpty());
    repolish(m_manageHint);
}

const Slot *MainWindow::currentSlot() const
{
    const QModelIndex index = m_grid->currentIndex();
    return index.isValid() ? m_model->slotAt(index.row()) : nullptr;
}

QString MainWindow::nextFreeSlotName() const
{
    QStringList used;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        if (const Slot *slot = m_model->slotAt(row))
            used << slot->name;
    }
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        const QString candidate(QChar::fromLatin1(letter));
        if (!used.contains(candidate))
            return candidate;
    }
    return {};
}

void MainWindow::bookSelected()
{
    const Slot *slot = currentSlot();
    if (!slot || slot->booked)
        return;
    const QString name = slot->name; // copy: the pointer dies on reload()

    if (const auto result = m_repository.book(name, m_account, QDateTime::currentDateTimeUtc()); !result) {
        showError(tr("Could not book slot %1:\n%2").arg(name, result.error()));
    } else {
        statusBar()->showMessage(tr("Slot %1 booked for you.").arg(name), 4000);
    }
    reload();
}

void MainWindow::cancelSelected()
{
    const Slot *slot = currentSlot();
    if (!slot || !slot->booked)
        return;
    const QString name = slot->name;
    const bool mine = slot->ownerId == m_account.id;

    if (!mine && !m_account.isAdmin) {
        statusBar()->showMessage(tr("Slot %1 was booked by someone else.").arg(name), 4000);
        return;
    }

    const QString question = mine
        ? tr("Release your booking on slot %1 so others can book it?").arg(name)
        : tr("Release slot %1, booked by %2, so others can book it?")
              .arg(name, slot->owner.isEmpty() ? tr("someone") : slot->owner);
    if (QMessageBox::question(this, tr("Release booking"), question) != QMessageBox::Yes)
        return;

    if (const auto result = m_repository.cancel(name, m_account); !result)
        showError(tr("Could not release slot %1:\n%2").arg(name, result.error()));
    else
        statusBar()->showMessage(tr("Slot %1 is available again.").arg(name), 4000);
    reload();
}

void MainWindow::activateSelected()
{
    const Slot *slot = currentSlot();
    if (!slot)
        return;
    if (slot->booked)
        cancelSelected();
    else
        bookSelected();
}

void MainWindow::addSlot()
{
    if (!m_account.isAdmin)
        return;

    const QString typed = m_slotNameEdit->text().trimmed();
    const QString name = typed.isEmpty() ? nextFreeSlotName() : typed.toUpper();

    if (name.isEmpty()) {
        setManageHint(tr("Every letter is already used. Type a name such as Q1."), true);
        return;
    }
    if (const auto result = m_repository.addSlot(name, m_account); !result) {
        setManageHint(result.error(), true);
        return;
    }

    m_slotNameEdit->clear();
    setManageHint(tr("Slot %1 added.").arg(name), false);
    reload();
    selectSlotByName(name.trimmed().toUpper());
}

void MainWindow::deleteSelected()
{
    const Slot *slot = currentSlot();
    if (!slot || slot->booked || !m_account.isAdmin)
        return;
    const QString name = slot->name;

    if (QMessageBox::question(this, tr("Delete slot"),
                              tr("Delete slot %1? It will disappear from the parking area. This cannot be undone.")
                                  .arg(name)) != QMessageBox::Yes)
        return;

    if (const auto result = m_repository.removeSlot(name, m_account); !result) {
        setManageHint(result.error(), true);
    } else {
        setManageHint(tr("Slot %1 deleted.").arg(name), false);
        statusBar()->showMessage(tr("Slot %1 deleted.").arg(name), 4000);
    }
    reload();
}

void MainWindow::newUserAccount()
{
    if (!m_account.isAdmin)
        return;
    AccountDialog dialog(m_repository, m_account, this);
    if (dialog.exec() == QDialog::Accepted)
        statusBar()->showMessage(tr("Account created."), 4000);
}

void MainWindow::signOut()
{
    hide();
    LoginDialog dialog(m_repository);
    if (dialog.exec() != QDialog::Accepted) {
        close(); // cancelled at the sign-in screen: quit
        return;
    }
    m_account = dialog.account();
    applyAccount();
    reload();
    show();
}

void MainWindow::showError(const QString &message)
{
    statusBar()->showMessage(message, 6000);
    QMessageBox::warning(this, tr("Bike Parking"), message);
}
