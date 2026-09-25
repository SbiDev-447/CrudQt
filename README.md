# CrudQt — CRUD de Estudiantes con Qt y SQLite

Aplicación de escritorio en C++/Qt que registra y consulta estudiantes con sus calificaciones, protegida por un login de administradores. Pensada como proyecto de aprendizaje de Qt: layouts responsivos, modelos SQL, validaciones y control de acceso con roles.

**Tabla de contenidos**
1. [Empezar](#empezar)
2. [Estructura del proyecto](#estructura-del-proyecto)
3. [Flujo de la aplicación](#flujo-de-la-aplicación)
4. [Modelo de datos](#modelo-de-datos)
5. [Reglas de negocio](#reglas-de-negocio)
6. [Seguridad](#seguridad)

---

## Empezar

**Requisitos:** CMake ≥ 3.16, Qt 6 (o Qt 5.15), compilador C++17. Módulo Qt **Widgets** y **Sql**.

```bash
cmake -S . -B build
cmake --build build
./build/CrudQt
```

Credenciales iniciales: **admin / admin** (administrador principal).

> La base de datos se crea automáticamente en la primera ejecución en `~/.local/share/CrudQt/crudqt.db` (Linux). No se recrea al abrir/cerrar la app; si el archivo se borra, el sistema la vuelve a crear con el admin principal.

## Estructura del proyecto

| Archivo | Responsabilidad |
|---------|-----------------|
| `main.cpp` | Inicializa la BD, muestra el login y luego la ventana principal. |
| `database.h/.cpp` | Abre/migra/siembra la BD (idempotente) y expone `hashPassword()`. |
| `logindialog.*` | Pantalla de inicio de sesión (validación contra `usuarios`). |
| `mainwindow.*` | Ventana principal: tabla de estudiantes, botones de acción, modelo `StudentTableModel`. |
| `studentdialog.*` | Diálogo agregar/editar estudiante con validaciones. |
| `admindialog.*` | Gestión de administradores: agregar, editar y eliminar (nunca el principal). |
| `rowactiondelegate.*` | Delegado que pinta el botón "Editar" en la fila seleccionada de la tabla. |
| `CMakeLists.txt` | Build: Qt Widgets + Sql, C++17, AUTOUIC/AUTOMOC/AUTORCC. |

## Flujo de la aplicación

```
main.cpp
  └─ initDatabase()   → abre BD, crea/migra tablas, siembra admin principal
  └─ LoginDialog      → valida usuario/password contra SQLite
  └─ MainWindow       → tabla de estudiantes + acciones
       ├─ Agregar     → StudentDialog (alta)
       ├─ Editar      → StudentDialog (edición; también con doble clic o botón de fila)
       ├─ Administradores → AdminDialog
       └─ Refrescar   → recarga la tabla
```

- **StudentTableModel** extiende `QSqlTableModel` y añade una **columna virtual "Acciones"** (no existe en la BD) para el botón de edición por fila.
- **RowActionDelegate** dibuja el botón "Editar" solo en la fila seleccionada; el clic abre el diálogo de edición y el doble clic en cualquier fila también edita.
- Todos los diálogos usan layouts: contenido centrado, `Enter` acepta, `Tab` ordenado, mensajes de error claros.

## Modelo de datos

**`usuarios`** — administradores del sistema:

| Columna | Tipo | Regla |
|---------|------|-------|
| `id` | INTEGER | PK autoincrement |
| `usuario` | TEXT | NOT NULL, UNIQUE |
| `password` | TEXT | NOT NULL, hash SHA-256 hex |
| `rol` | TEXT | `principal` o `admin` (default `admin`) |

**`estudiantes`** — registros del CRUD:

| Columna | Tipo | Regla |
|---------|------|-------|
| `id` | INTEGER | PK autoincrement |
| `nombre` / `apellido` | TEXT | NOT NULL |
| `cedula` | TEXT | NOT NULL, UNIQUE (anti-duplicado) |
| `trayecto` / `tramo` / `seccion` | TEXT | NOT NULL (listas fijas en la UI) |
| `calificacion` | REAL | NULL = "Sin calificar"; escala 0–20 |
| `creado_en` | TEXT | DEFAULT `datetime('now')` |

## Reglas de negocio

| Regla | Implementación |
|-------|----------------|
| Sin duplicados | `cedula UNIQUE` en BD + consulta previa antes de insertar/editar. |
| Sin valores nulos | `NOT NULL` en BD + validación en UI (mensaje y foco en el campo). |
| Datos parecidos permitidos | La unicidad aplica solo a la cédula; el resto puede repetirse. |
| Estudiantes no se eliminan | No hay botón ni sentencia `DELETE` para `estudiantes`. |
| Editar calificación | El diálogo de edición permite cambiarla en cualquier momento. |
| Administradores editables | Se puede cambiar usuario y/o contraseña de cualquier admin. |
| Principal blindado | UI deshabilita editar/eliminar al `principal` + guard SQL `rol != 'principal'`. |
| BD persistente | No se recrea en cada apertura; se autorecrea si el archivo desaparece. |

## Seguridad

- **Consultas preparadas** (`bindValue`) en todo el código → sin inyección SQL.
- **Contraseñas** hasheadas con SHA-256 hex (mismo algoritmo en login, seed y gestión de admins).
- **Blindaje en dos capas**: la UI oculta/deshabilita acciones sobre el principal **y** la BD rechaza cualquier `UPDATE`/`DELETE` que lo toque.

> Nota: SHA-256 sin *salt* es suficiente para este proyecto educativo, pero no para producción. Para un sistema real, usa PBKDF2 (`QPasswordDigestor`) o Argon2/bcrypt.

## Verificación rápida

- [ ] `cmake --build build` compila sin errores.
- [ ] `admin / admin` accede al sistema.
- [ ] Agregar estudiante; repetir la misma cédula → la BD rechaza (UNIQUE).
- [ ] Dejar campos vacíos → la UI avisa y enfoca el campo.
- [ ] Maximizar la ventana → botones agrupados y tabla repartiendo el ancho.
- [ ] Borrar `crudqt.db` y abrir la app → se recrea sola con `admin`.