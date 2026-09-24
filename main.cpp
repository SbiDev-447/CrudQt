#include "database.h"
#include "logindialog.h"
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  // Abre, migra y siembra la BD (idempotente).
  if (!initDatabase()) {
    return 1;
  }

  // Login primero
  LoginDialog login;
  if (login.exec() != QDialog::Accepted) {
    return 0; // Usuario canceló o cerró el login
  }

  // CRUD después
  MainWindow w;
  w.show();

  return a.exec();
}