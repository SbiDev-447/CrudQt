#include "database.h"
#include "logindialog.h"
#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QDialog>

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  // Tema y paleta ANTES de cualquier diálogo o ventana: el login ya sale con el
  // tema elegido. Se lee de QSettings una sola vez; si el usuario cambia de
  // tema más adelante, MainWindow lo vuelve a aplicar.
  aplicarTema(temaOscuroGuardado());

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
