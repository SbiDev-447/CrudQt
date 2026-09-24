#ifndef DATABASE_H
#define DATABASE_H

#include <QString>

// Inicializa la base de datos de forma idempotente: abre la BD en
// AppDataLocation, migra la tabla usuarios (columna rol) y asegura la tabla
// estudiantes con el admin principal sembrado. Devuelve false si falla.
bool initDatabase();

// SHA-256 hex del password (mismo algoritmo que usa el login).
QString hashPassword(const QString &password);

#endif // DATABASE_H