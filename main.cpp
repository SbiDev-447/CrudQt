#include "logindialog.h"
#include "mainwindow.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  // --- Configurar la base de datos ---
  QString dirDatos =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dirDatos); // Crear la carpeta si no existe
  QString rutaDb = dirDatos + "/crudqt.db";

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
  db.setDatabaseName(rutaDb);

  if (!db.open()) {
    qCritical() << "No se pudo abrir la BD:" << db.lastError().text();
    return 1;
  }

  // --- Crear la tabla de usuarios si no existe ---
  QSqlQuery query;
  if (!query.exec("CREATE TABLE IF NOT EXISTS usuarios ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "usuario TEXT NOT NULL UNIQUE, "
                  "password TEXT NOT NULL)")) {
    qCritical() << "Error creando tabla usuarios:" << query.lastError().text();
    return 1;
  }

  // --- Usuario semilla: admin / admin (solo si no existe) ---
  QSqlQuery seed;
  seed.prepare("INSERT OR IGNORE INTO usuarios (usuario, password) "
               "VALUES (:u, :p)");
  QByteArray hashSemilla =
      QCryptographicHash::hash(QByteArray("admin"), QCryptographicHash::Sha256)
          .toHex();
  seed.bindValue(":u", "admin");
  seed.bindValue(":p", QString(hashSemilla));
  if (!seed.exec()) {
    qCritical() << "Error creando usuario semilla:" << seed.lastError().text();
    return 1;
  }

  // --- Login primero ---
  LoginDialog login;
  if (login.exec() != QDialog::Accepted) {
    return 0; // Usuario canceló o cerró el login
  }

  // --- CRUD después ---
  MainWindow w;
  w.show();

  return a.exec();
}
