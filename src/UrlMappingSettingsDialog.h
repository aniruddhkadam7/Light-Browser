#pragma once

#include <QDialog>

class QTableWidget;
class UrlRedirectManager;

// Settings UI for UrlRedirectManager's rules: add/edit/delete/enable.
class UrlMappingSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UrlMappingSettingsDialog(UrlRedirectManager *manager, QWidget *parent = nullptr);

private:
    void refreshTable();
    void addRule();
    void editRule(int row);
    void deleteRule(int row);

    UrlRedirectManager *m_manager;
    QTableWidget *m_table;
};
