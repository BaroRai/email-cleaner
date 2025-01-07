#include "AddAccountDialog.h"
#include "src/MainWindow/ui_addaccountdialog.h"

AddAccountDialog::AddAccountDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::AddAccountDialog) {
    ui->setupUi(this);

    // Button func
    connect(ui->okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

AddAccountDialog::~AddAccountDialog() {
    delete ui;
}

QString AddAccountDialog::getServer() const {
    return ui->serverLineEdit->toPlainText();
}

QString AddAccountDialog::getUsername() const {
    return ui->usernameLineEdit->toPlainText();
}

QString AddAccountDialog::getPassword() const {
    return ui->passwordLineEdit->toPlainText();
}
