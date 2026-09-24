#include "logindialog.h"
#include "ui_logindialog.h"
#include <QCryptographicHash>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::LoginDialog) {
  ui->setupUi(this);
}

LoginDialog::~LoginDialog() { delete ui; }

void LoginDialog::on_btnIngresar_clicked() {
  QString usuario = ui->lineUsuario->text().trimmed();
  QString password = ui->linePassword->text();

  if (usuario.isEmpty() || password.isEmpty()) {
    QMessageBox::warning(this, "Campos vacíos",
                         "Ingresa usuario y contraseña.");
    return;
  }

  if (validarCredenciales(usuario, password)) {
    accept(); // Cierra el diálogo con QDialog::Accepted
  } else {
    QMessageBox::warning(this, "Acceso denegado",
                         "Usuario o contraseña incorrectos.");
    ui->linePassword->clear();
    ui->linePassword->setFocus();
  }
}

void LoginDialog::on_btnCancelar_clicked() {
  reject(); // Cierra con QDialog::Rejected
}

bool LoginDialog::validarCredenciales(const QString &usuario,
                                      const QString &password) {
  // Hashear la contraseña antes de comparar
  QByteArray hash =
      QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256)
          .toHex();

  QSqlQuery query;
  query.prepare("SELECT id FROM usuarios "
                "WHERE usuario = :u AND password = :p");
  query.bindValue(":u", usuario);
  query.bindValue(":p", QString(hash));

  if (!query.exec()) {
    QMessageBox::critical(this, "Error de BD", query.lastError().text());
    return false;
  }

  return query.next(); // true si encontró una fila
}
