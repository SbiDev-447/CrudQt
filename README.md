# CrudQt — CRUD de Estudiantes con Qt y SQLite

Aplicación de escritorio en C++/Qt que registra y consulta estudiantes con sus calificaciones, protegida por un login de administradores. Pensada como proyecto de aprendizaje de Qt: layouts responsivos, modelos SQL, validaciones, control de acceso con roles y un tema visual con dos variantes que el usuario elige desde el menú de ajustes.

**Tabla de contenidos**
1. [Empezar](#empezar)
2. [Compilar para Windows desde Linux](#compilar-para-windows-desde-linux)
3. [Estructura del proyecto](#estructura-del-proyecto)
4. [Flujo de la aplicación](#flujo-de-la-aplicación)
5. [Apariencia y ajustes](#apariencia-y-ajustes)
6. [Modelo de datos](#modelo-de-datos)
7. [Reglas de negocio](#reglas-de-negocio)
8. [Seguridad](#seguridad)
9. [Documentación](#documentación)
10. [Verificación rápida](#verificación-rápida)

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

## Compilar para Windows desde Linux

Con Qt no se puede reutilizar el Qt del sistema: cada plataforma trae sus binarios (ELF/`.so` en Linux, PE/`.dll` en Windows). Para generar un ejecutable de Windows desde Linux se necesita un compilador que produzca PE (LLVM-MinGW) y un Qt compilado para Windows, además de un Qt de host para ejecutar `moc`, que es un binario PE que Linux no puede lanzar. Todo el toolchain se instala de forma portátil en `~/devtools`, sin tocar el sistema:

```bash
# 1) Qt de Windows (bibliotecas del exe), Qt de Linux (herramientas) y compilador
PYTHONPATH=~/devtools/aqt-pylib python3 -m aqt install-qt windows desktop 6.8.2 win64_mingw  -O ~/devtools
PYTHONPATH=~/devtools/aqt-pylib python3 -m aqt install-qt linux   desktop 6.8.2 linux_gcc_64 -O ~/devtools

# 2) Configurar, 3) compilar, 4) empaquetar
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-x86_64.cmake \
  -DQT_HOST_PATH=$HOME/devtools/6.8.2/gcc_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-win -j$(nproc)
bash deploy-linux-cross.sh
```

El resultado es **`dist/CrudQt-win-x86_64.zip`** (13 MB): el usuario descomprime y ejecuta `CrudQt.exe` sin instalar nada. La guía completa —toolchain, empaquetado, verificación y resolución de problemas— está en **[docs/forWindowsBuilt.md](docs/forWindowsBuilt.md)**.

## Estructura del proyecto

| Archivo | Responsabilidad |
|---------|-----------------|
| `main.cpp` | Aplica el tema guardado, inicializa la BD y repite el ciclo login → ventana principal mientras el usuario no cierre la app. |
| `database.h/.cpp` | Abre/migra/siembra la BD (idempotente) y expone `hashPassword()`. |
| `logindialog.*` | Pantalla de inicio de sesión (validación contra `usuarios`). |
| `mainwindow.*` | Ventana principal: tabla de estudiantes, botones de acción, menú de ajustes, modelo `StudentTableModel`. |
| `studentdialog.*` | Diálogo agregar/editar estudiante con validaciones. |
| `admindialog.*` | Gestión de administradores: agregar, editar y eliminar (nunca el principal). |
| `rowactiondelegate.*` | Delegado que pinta el botón "Editar" en la fila seleccionada de la tabla. |
| `theme.h/.cpp` | Tema visual: aplica estilo Fusion, la paleta y la hoja de estilos del tema elegido (oscuro o claro). |
| `style-dark.qss` / `style-light.qss` | Hojas de estilos de cada variante, empaquetadas en el binario por `resources.qrc`. |
| `CMakeLists.txt` | Build: Qt Widgets + Sql, C++17, AUTOUIC/AUTOMOC/AUTORCC. |

## Flujo de la aplicación

```
main.cpp
  └─ aplicarTema(temaOscuroGuardado()) → tema de QSettings aplicado ANTES del login
  └─ initDatabase()      → abre BD, crea/migra tablas, siembra admin principal
  └─ while (true)                     ← bucle login → app
       ├─ LoginDialog   → valida usuario/password contra SQLite
       └─ MainWindow    → tabla de estudiantes + acciones
            ├─ Agregar          → StudentDialog (alta)
            ├─ Editar           → StudentDialog (edición; también con doble clic o botón de fila)
            ├─ Refrescar        → recarga la tabla
            └─ Menú Ajustes (QToolButton, esquina superior izquierda)
                 ├─ Tema oscuro / Tema claro → cambiarTema() y se persiste
                 ├─ Administradores...      → AdminDialog
                 └─ Cerrar sesión           → vuelve al login (no cierra la app)
  └─ cerrar la ventana con la X → termina la aplicación
```

- **StudentTableModel** extiende `QSqlTableModel` y añade una **columna virtual "Acciones"** (no existe en la BD) para el botón de edición por fila.
- **RowActionDelegate** dibuja el botón "Editar" solo en la fila seleccionada; el clic abre el diálogo de edición y el doble clic en cualquier fila también edita.
- "Cerrar sesión" destruye la ventana principal y vuelve a mostrar el login en la misma instancia del programa; la base de datos sigue abierta y no se reinicializa.
- Todos los diálogos usan layouts: contenido centrado, `Enter` acepta, `Tab` ordenado, mensajes de error claros.

## Apariencia y ajustes

**Tema visual.** La app usa la paleta Gruvbox en dos variantes con la misma estructura visual: **tema oscuro** (el predeterminado) y **tema claro**. El tema no está atado a ningún widget: se aplica a nivel de aplicación (estilo Fusion + paleta + hoja de estilos), por lo que login, ventana principal y diálogos cambian de aspecto a la vez.

**Menú "Ajustes".** En la esquina superior izquierda de la ventana principal hay un botón **Ajustes** con un menú que agrupa las acciones de sesión:

| Acción | Efecto |
|--------|--------|
| Tema oscuro / Tema claro | Cambia el tema al instante. Son excluyentes: la acción marcada es la activa. |
| Administradores... | Abre `AdminDialog` (gestión de administradores). |
| Cerrar sesión | Cierra la ventana principal y vuelve a la pantalla de login. |

**Persistencia.** La preferencia se guarda en `QSettings` (organización/aplicación `CrudQt`/`CrudQt`, clave `tema`, valores `dark`/`light`). Si no hay nada guardado, la app arranca con el tema oscuro. El tema elegido se aplica antes de mostrar el login, así que la primera pantalla ya aparece con el tema elegido.

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

## Documentación

Las tres guías de estudio viven en `docs/`. El README dice **qué hace** el sistema; las guías explican **cómo** y **por qué**.

| Documento | Para qué sirve |
|---|---|
| [Explication.md](docs/Explication.md) | Guía de estudio del sistema completo: arquitectura, Model/View, ciclo de construcción con CMake, tema visual, diálogos, ciclo de sesión, reglas de negocio, seguridad y guiones end-to-end. Termina con un checklist de autoevaluación. |
| [comoArmarTu-DB-EnSQLite.md](docs/comoArmarTu-DB-EnSQLite.md) | Guía de SQLite con el esquema real del proyecto: tablas, tipos y restricciones columna por columna, la distinción `NOT NULL` frente a `NULL` que produce "Sin calificar", el flujo de `initDatabase()`, la migración de la columna `rol`, todas las consultas que la app ejecuta y cómo inspeccionar `crudqt.db` con la CLI o con Qt Creator. |
| [forWindowsBuilt.md](docs/forWindowsBuilt.md) | Guía para obtener `dist/CrudQt-win-x86_64.zip` desde Linux: toolchain portátil en `~/devtools`, configuración, compilación cruzada, empaquetado, verificación y resolución de problemas. |

## Verificación rápida

- [ ] `cmake --build build` compila sin errores.
- [ ] `admin / admin` accede al sistema.
- [ ] Agregar estudiante; repetir la misma cédula → la BD rechaza (UNIQUE).
- [ ] Dejar campos vacíos → la UI avisa y enfoca el campo.
- [ ] Maximizar la ventana → botones agrupados y tabla repartiendo el ancho.
- [ ] Ajustes → Tema claro; cerrar y abrir la app → el tema claro se mantiene.
- [ ] Ajustes → Cerrar sesión → vuelve al login y permite entrar de nuevo.
- [ ] Cerrar la ventana principal con la X → la aplicación termina.
- [ ] Borrar `crudqt.db` y abrir la app → se recrea sola con `admin`.
