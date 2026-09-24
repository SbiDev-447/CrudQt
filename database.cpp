#include "database.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

// Agrega la columna rol a usuarios SOLO si falta (BD existentes pre-migración).
bool migrarColumnaRol() {
  QSqlQuery tableInfo("PRAGMA table_info(usuarios)");
  while (tableInfo.next()) {
    if (tableInfo.value(1).toString() == "rol") {
      return true; // ya existe, nada que migrar
    }
  }

  QSqlQuery alter;
  if (!alter.exec("ALTER TABLE usuarios "
                  "ADD COLUMN rol TEXT NOT NULL DEFAULT 'admin'")) {
    qCritical() << "Error migrando columna rol:" << alter.lastError().text();
    return false;
  }
  return true;
}

} // namespace

bool initDatabase() {
  // Ruta de la BD: ~/.local/share/CrudQt/crudqt.db en Linux.
  const QString dirDatos =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dirDatos); // Crear la carpeta si no existe
  const QString rutaDb = dirDatos + "/crudqt.db";

  // Conexión por defecto, creada una sola vez.
  if (!QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
    QSqlDatabase::addDatabase("QSQLITE");
  }
  QSqlDatabase db = QSqlDatabase::database();
  db.setDatabaseName(rutaDb);

  if (!db.open()) {
    qCritical() << "No se pudo abrir la BD:" << db.lastError().text();
    return false;
  }

  QSqlQuery query;

  // Tabla usuarios con rol incluido (BD nuevas).
  if (!query.exec("CREATE TABLE IF NOT EXISTS usuarios ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "usuario TEXT NOT NULL UNIQUE, "
                  "password TEXT NOT NULL, "
                  "rol TEXT NOT NULL DEFAULT 'admin')")) {
    qCritical() << "Error creando tabla usuarios:" << query.lastError().text();
    return false;
  }

  // Migración idempotente para BD existentes sin la columna rol.
  if (!migrarColumnaRol()) {
    return false;
  }

  // Tabla estudiantes: solo cedula es única; calificación nullable.
  if (!query.exec("CREATE TABLE IF NOT EXISTS estudiantes ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "nombre TEXT NOT NULL, "
                  "apellido TEXT NOT NULL, "
                  "cedula TEXT NOT NULL UNIQUE, "
                  "trayecto TEXT NOT NULL, "
                  "tramo TEXT NOT NULL, "
                  "seccion TEXT NOT NULL, "
                  "calificacion REAL NULL, "
                  "creado_en TEXT DEFAULT (datetime('now')))")) {
    qCritical() << "Error creando tabla estudiantes:"
                << query.lastError().text();
    return false;
  }

  // Usuario semilla: admin / admin con rol principal (solo si no existe).
  QSqlQuery seed;
  seed.prepare("INSERT OR IGNORE INTO usuarios (usuario, password, rol) "
               "VALUES (:u, :p, 'principal')");
  seed.bindValue(":u", "admin");
  seed.bindValue(":p", hashPassword("admin"));
  if (!seed.exec()) {
    qCritical() << "Error creando usuario semilla:" << seed.lastError().text();
    return false;
  }

  // Asegura que el admin semilla tenga rol principal (BD migradas).
  QSqlQuery ensureAdmin;
  if (!ensureAdmin.exec(
          "UPDATE usuarios SET rol = 'principal' WHERE usuario = 'admin'")) {
    qCritical() << "Error asegurando admin principal:"
                << ensureAdmin.lastError().text();
    return false;
  }

  return true;
}

QString hashPassword(const QString &password) {
  return QString(QCryptographicHash::hash(password.toUtf8(),
                                          QCryptographicHash::Sha256)
                     .toHex());
}