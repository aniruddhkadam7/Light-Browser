#include "UrlMappingSettingsDialog.h"
#include "UrlRedirectManager.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

// Small modal for entering/editing one rule's source + target patterns.
class RuleEditDialog : public QDialog
{
public:
    explicit RuleEditDialog(QWidget *parent, const QString &source = QString(),
                             const QString &target = QString())
        : QDialog(parent)
    {
        setWindowTitle(tr("URL Mapping Rule"));
        setMinimumWidth(420);

        auto *layout = new QVBoxLayout(this);

        layout->addWidget(new QLabel(tr("Source pattern (e.g. https://app.company.com/*)"), this));
        m_sourceEdit = new QLineEdit(source, this);
        layout->addWidget(m_sourceEdit);

        layout->addWidget(new QLabel(tr("Target pattern (e.g. https://staging.company.com/*)"), this));
        m_targetEdit = new QLineEdit(target, this);
        layout->addWidget(m_targetEdit);

        auto *hint = new QLabel(
            tr("Use one '*' in each pattern to carry over the path, query, and fragment."), this);
        hint->setWordWrap(true);
        hint->setStyleSheet("color: #9c9c9c; font-size: 11px;");
        layout->addWidget(hint);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            if (m_sourceEdit->text().trimmed().isEmpty() || m_targetEdit->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, tr("URL Mapping Rule"),
                                      tr("Both source and target patterns are required."));
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    QString source() const { return m_sourceEdit->text().trimmed(); }
    QString target() const { return m_targetEdit->text().trimmed(); }

private:
    QLineEdit *m_sourceEdit;
    QLineEdit *m_targetEdit;
};

} // namespace

UrlMappingSettingsDialog::UrlMappingSettingsDialog(UrlRedirectManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle(tr("URL Mapping Rules"));
    resize(640, 360);

    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(
        tr("Navigating to a source URL loads the mapped target instead."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("On"), tr("Source"), tr("Target"), QString(), QString()});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(m_table);

    auto *buttonRow = new QHBoxLayout();
    auto *addButton = new QPushButton(tr("Add Rule"), this);
    connect(addButton, &QPushButton::clicked, this, &UrlMappingSettingsDialog::addRule);
    buttonRow->addWidget(addButton);
    buttonRow->addStretch(1);
    auto *closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    refreshTable();
}

void UrlMappingSettingsDialog::refreshTable()
{
    const QVector<RedirectRule> rules = m_manager->rules();
    m_table->setRowCount(rules.size());

    for (int row = 0; row < rules.size(); ++row) {
        const RedirectRule &rule = rules[row];
        const QString id = rule.id;

        auto *enabledCheck = new QCheckBox(m_table);
        enabledCheck->setChecked(rule.enabled);
        connect(enabledCheck, &QCheckBox::toggled, this,
                [this, id](bool checked) { m_manager->setEnabled(id, checked); });
        auto *checkWrap = new QWidget(m_table);
        auto *checkLayout = new QHBoxLayout(checkWrap);
        checkLayout->addWidget(enabledCheck);
        checkLayout->setAlignment(Qt::AlignCenter);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        m_table->setCellWidget(row, 0, checkWrap);

        m_table->setItem(row, 1, new QTableWidgetItem(rule.source));
        m_table->setItem(row, 2, new QTableWidgetItem(rule.target));

        auto *editButton = new QPushButton(tr("Edit"), m_table);
        connect(editButton, &QPushButton::clicked, this, [this, row] { editRule(row); });
        m_table->setCellWidget(row, 3, editButton);

        auto *deleteButton = new QPushButton(tr("Delete"), m_table);
        connect(deleteButton, &QPushButton::clicked, this, [this, row] { deleteRule(row); });
        m_table->setCellWidget(row, 4, deleteButton);
    }
}

void UrlMappingSettingsDialog::addRule()
{
    RuleEditDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_manager->addRule(dlg.source(), dlg.target(), true);
        refreshTable();
    }
}

void UrlMappingSettingsDialog::editRule(int row)
{
    const QVector<RedirectRule> rules = m_manager->rules();
    if (row < 0 || row >= rules.size())
        return;
    const RedirectRule rule = rules[row];

    RuleEditDialog dlg(this, rule.source, rule.target);
    if (dlg.exec() == QDialog::Accepted) {
        m_manager->updateRule(rule.id, dlg.source(), dlg.target(), rule.enabled);
        refreshTable();
    }
}

void UrlMappingSettingsDialog::deleteRule(int row)
{
    const QVector<RedirectRule> rules = m_manager->rules();
    if (row < 0 || row >= rules.size())
        return;
    const RedirectRule rule = rules[row];

    const auto reply = QMessageBox::question(this, tr("Delete Rule"),
                                              tr("Delete the mapping rule for \"%1\"?").arg(rule.source));
    if (reply == QMessageBox::Yes) {
        m_manager->removeRule(rule.id);
        refreshTable();
    }
}
