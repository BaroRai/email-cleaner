#ifndef ADDACCOUNTDIALOG_H
#define ADDACCOUNTDIALOG_H

#include <QDialog>

namespace Ui {
class AddAccountDialog;
}

class AddAccountDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddAccountDialog(QWidget *parent = nullptr);
    ~AddAccountDialog();

    QString getServer() const;
    QString getUsername() const;
    QString getPassword() const;

private:
    Ui::AddAccountDialog *ui;
};

#endif // ADDACCOUNTDIALOG_H
