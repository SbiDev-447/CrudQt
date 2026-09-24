#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog {
  Q_OBJECT

public:
  explicit LoginDialog(QWidget *parent = nullptr);
  ~LoginDialog();

private slots:
  void on_btnIngresar_clicked();
  void on_btnCancelar_clicked();

private:
  Ui::LoginDialog *ui;
  bool validarCredenciales(const QString &usuario, const QString &password);
};

#endif // LOGINDIALOG_H
