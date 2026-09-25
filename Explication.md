# Explication — Guía de estudio del sistema CrudQt (Qt/C++)

Este documento explica cómo está construido CrudQt y **por qué** está diseñado así. Es complementario al README: el README dice *qué hace* el sistema; esta guía dice *cómo* lo hace el código y qué concepto de Qt enseña cada pieza.

Todos los nombres de funciones, métodos, consultas SQL y condiciones citados aquí existen literalmente en el código del proyecto. Si lees con el código abierto al lado, cada afirmación se puede verificar.

---

## 1. Qué es el sistema

CrudQt es una aplicación de escritorio que registra y consulta estudiantes con sus calificaciones, protegida por un login de administradores con roles. Stack: **Qt 6 / Qt 5.15 (módulos Widgets y Sql), C++17, SQLite y CMake**. Es un proyecto pensado para aprender Qt: en una sola base de código se conectan el patrón **Model/View** (`QSqlTableModel` + `QTableView` + un delegado personalizado), **signal/slot** (auto-conexiones por nombre *y* conexiones manuales), **layouts** creados con Qt Designer, **consultas preparadas** (anti inyección SQL) y **roles de acceso** (administrador normal vs. principal). Además muestra decisiones de diseño que un estudiante debe aprender a razonar: una columna de tabla que **no existe en la base de datos**, un botón por fila que se **pinta condicionalmente** y una base de datos que se **migra sola** sin perder datos.

---

## 2. Mapa de arquitectura

Flujo de arranque y navegación:

```
main()
 ├─ initDatabase()                  database.cpp   (abre/crea/migra/siembra la BD)
 ├─ LoginDialog login; login.exec() logindialog.*  (modal; valida credenciales)
 │      └─ validarCredenciales()
 ├─ MainWindow w; w.show()          mainwindow.*   (CRUD de estudiantes)
 │      ├─ StudentTableModel + QTableView          (Model/View)
 │      ├─ RowActionDelegate                       (botón "Editar" por fila)
 │      ├─ StudentDialog                           (alta/edición de estudiantes)
 │      └─ AdminDialog                             (gestión de administradores)
 └─ a.exec()                                      (bucle de eventos)
```

| Archivo | Rol en el sistema | Conceptos Qt que enseña |
|---|---|---|
| `main.cpp` | Orquesta el arranque: BD, login, ventana | `QApplication`, `QDialog::exec()`, vida de la app |
| `database.h/.cpp` | Abre, migra y siembra SQLite (idempotente); `hashPassword()` | `QSqlDatabase`, `QSqlQuery`, `QStandardPaths`, `QCryptographicHash`, ORM manual con SQL crudo |
| `logindialog.*` | Login contra la tabla `usuarios` | `QDialog` modal, auto-conexiones `on_*`, consultas preparadas |
| `mainwindow.*` | Ventana principal, tabla de estudiantes | Model/View: `QSqlTableModel` + subclase, `QHeaderView`, delegados por columna |
| `studentdialog.*` | Alta/edición de estudiantes | Reuso de un diálogo para dos modos (INSERT/UPDATE), validaciones |
| `admindialog.*` | CRUD de administradores (principal blindado) | `QTableWidget` programático, `Qt::UserRole`, doble capa de protección |
| `rowactiondelegate.*` | Botón "Editar" en la fila seleccionada | `QStyledItemDelegate`: `paint`, `editorEvent`, señales propias |

---

## 3. Cómo se construye (CMake)

El `CMakeLists.txt` es la "receta" que convierte el código fuente en un ejecutable. Lo importante para un estudiante:

**Los tres "autos"** (líneas 5–7) activan generadores automáticos de código:

```cmake
set(CMAKE_AUTOUIC ON)   # uic:  .ui (XML de Designer) -> ui_*.h
set(CMAKE_AUTOMOC ON)   # moc:  header con Q_OBJECT -> moc_*.cpp
set(CMAKE_AUTORCC ON)   # rcc:  .qrc (recursos) -> qrc_*.cpp
```

- **AUTOUIC**: cuando compilas `logindialog.cpp` (que incluye `ui_logindialog.h`), CMake detecta `logindialog.ui` y ejecuta **uic**, que convierte el XML en la clase `Ui::LoginDialog` con un método `setupUi()`. Ese `setupUi()` crea los widgets, aplica las propiedades (textos, layouts, `echoMode`, `tabstops`) y, crucialmente, llama a `QMetaObject::connectSlotsByName()`. Por eso **los archivos `.ui` tienen `<connections/>` vacío**: las conexiones de botones no se declaran en Designer, se derivan de los nombres. Si cambias un `.ui` (por ejemplo, añades un campo), uic regenera `ui_*.h` y el compilador lo recoge en el siguiente build.
- **AUTOMOC**: cualquier header del proyecto que declare `Q_OBJECT` (todos los diálogos, `MainWindow`, `StudentTableModel`, `RowActionDelegate`) necesita que **moc** genere su meta-objeto (`staticMetaObject`), las implementaciones de las señales y el registro de slots para auto-conexión. Sin `Q_OBJECT` no hay `signals:`, no hay `connect` por nombre ni `qobject_cast` sobre esa clase.
- **AUTORCC**: compila recursos `.qrc`. CrudQt no usa recursos; la línea está por completitud del plantilla.

**Los módulos de Qt** (líneas 12–13):

```cmake
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql)
```

`find_package` busca Qt, define `QT_VERSION_MAJOR` y crea los targets importados `Qt6::Widgets` y `Qt6::Sql`. **El componente `Sql` es obligatorio**: `QSqlDatabase`, `QSqlQuery` y `QSqlError` viven en el módulo Qt Sql; si solo enlazaras Widgets, el código de `database.cpp` ni siquiera compilaría (headers fuera del include path y enlace faltante). La doble llamada con `Qt${QT_VERSION_MAJOR}` hace el proyecto compilable con Qt 6 o Qt 5.15.

**El target** (líneas 35–56): con Qt 6 se usa `qt_add_executable` con `MANUAL_FINALIZATION`, y al final `qt_finalize_executable` (las líneas 81–83). La finalización diferida es necesaria en plataformas como Android/iOS para ajustar el target *después* de todos los `set_property`; en escritorio es inofensiva pero es el patrón oficial. Con Qt 5 se cae al clásico `add_executable`.

**`WIN32_EXECUTABLE TRUE`** (línea 71): en Windows produce un ejecutable de subsistema gráfico (sin ventana de consola). En Linux/macOS no tiene efecto práctico. Es la misma propiedad que pone el plantilla de Qt Creator.

---

## 4. Inicialización de la base de datos (database.cpp)

**Respuesta corta: `initDatabase()` es idempotente** — puedes ejecutarlo mil veces y nunca duplica datos ni rompe nada. Esa propiedad es la que permite que la BD sobreviva a cada apertura de la app.

Paso a paso:

1. **Ruta**: `QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)` devuelve, en Linux, `~/.local/share/CrudQt/` (el nombre de la app se toma del ejecutable porque nadie llama a `setApplicationName`). `QDir().mkpath()` crea la carpeta si falta y el archivo queda en `~/.local/share/CrudQt/crudqt.db`.
2. **Conexión**: si no existe la conexión por defecto (`QSqlDatabase::contains(QSqlDatabase::defaultConnection)`), se registra el driver `"QSQLITE"`. El guard evita el error "duplicate connection name" si `initDatabase()` se llamara dos veces. Luego `setDatabaseName(rutaDb)` y `open()`; si falla, retorna `false` y `main()` termina con código 1.
3. **Tabla `usuarios`** con `CREATE TABLE IF NOT EXISTS`, ya con la columna `rol` incluida (para BD nuevas).
4. **Migración** (`migrarColumnaRol()`): consulta `PRAGMA table_info(usuarios)`. Si ya existe la columna `rol`, no hace nada; si no (BD creadas antes de introducir el rol), ejecuta `ALTER TABLE usuarios ADD COLUMN rol TEXT NOT NULL DEFAULT 'admin'`. El `PRAGMA` se consulta *antes* de alterar: la migración es idempotente por diseño.
5. **Tabla `estudiantes`** con `CREATE TABLE IF NOT EXISTS`: `cedula` es `UNIQUE` (anti duplicado), `calificacion REAL NULL` (permitido que un estudiante no tenga nota) y `creado_en` con `DEFAULT (datetime('now'))` que SQLite rellena solo (el INSERT no lo menciona).
6. **Semilla**: `INSERT OR IGNORE INTO usuarios (usuario, password, rol) VALUES (:u, :p, 'principal')` con `admin` y `hashPassword("admin")`. `OR IGNORE` + el `UNIQUE` de `usuario` lo convierten en "inserta solo si no existe".
7. **Asegurado del rol**: `UPDATE usuarios SET rol = 'principal' WHERE usuario = 'admin'`. Sirve para BD migradas cuyo admin existía desde antes del rol: garantiza que el semilla siempre sea el principal.

**¿Por qué la BD no se recrea en cada apertura?** Porque el archivo vive en `AppDataLocation`, fuera del directorio de build, y todas las sentencias son "si no existe". Recompilar o abrir la app no toca los datos. **¿Y si el archivo se borra?** En el próximo arranque `initDatabase()` recrea carpeta, tablas y admin semilla desde cero: el sistema vuelve a funcionar con `admin/admin`.

**`hashPassword()`** (database.cpp:108–112) calcula el SHA-256 del *UTF-8* del texto y lo devuelve en hexadecimal. Es la **única** función de hasheo del proyecto: la usan el seed, el login y la gestión de admins, de modo que el algoritmo nunca diverge entre piezas.

---

## 5. El login (logindialog.h/.cpp)

**Cómo se abre**: `LoginDialog` es un `QDialog`. `main.cpp` lo usa con `login.exec()`: un bucle de eventos anidado y modal. El retorno se compara con `QDialog::Accepted`; si el usuario cierra o cancela (`reject()`), el programa termina con `return 0` sin llegar al CRUD.

**Cómo se conectan los botones**: los slots `on_btnIngresar_clicked` y `on_btnCancelar_clicked` se auto-conectan. La convención es `on_<objectName>_<signal>`, y la magia la hacen dos piezas que vimos en la sección 3: el **moc** registra los slots en el meta-objeto y el `setupUi()` de uic llama a `connectSlotsByName()`, que los cose al `clicked()` de `btnIngresar` y `btnCancelar`. Por eso el `.ui` no declara ninguna conexión.

**Cómo valida** (`validarCredenciales`, logindialog.cpp:39–53):

```cpp
query.prepare("SELECT id FROM usuarios "
              "WHERE usuario = :u AND password = :p");
query.bindValue(":u", usuario);
query.bindValue(":p", hashPassword(password));
...
return query.next(); // true si encontró una fila
```

- **¿Por qué se compara el hash y no el texto?** La BD solo guarda hashes (seed y altas usan `hashPassword`). Comparar el texto en claro contra el hash sería siempre falso: hay que hashear la entrada con el mismo algoritmo.
- **¿Por qué consulta preparada?** `bindValue` parametriza: el texto del usuario jamás se concatena a la cadena SQL. Prueba mental de estudio: si escribes como usuario `' OR '1'='1`, entra *como dato*, no como SQL. Eso es la inyección SQL y así se elimina.
- Tras el éxito: `accept()` cierra el diálogo con `QDialog::Accepted`. Tras el fallo: aviso, se limpia el campo de contraseña y se devuelve el foco.

**Observación pedagógica**: el login no consulta `rol`. Cualquier usuario con credenciales válidas entra; el rol solo decide qué se puede hacer dentro de `AdminDialog`. Si quisieras permisos distintos por rol en el CRUD, aquí es donde tendrías que empezar a mirar.

---

## 6. La ventana principal y Model/View (mainwindow.h/.cpp)

**El corazón pedagógico del proyecto. Tres clases, tres responsabilidades:**

| Clase | Responsabilidad |
|---|---|
| `QSqlTableModel` / `StudentTableModel` | Los *datos*: qué filas/columnas hay, qué se muestra, qué es editable |
| `QTableView` | La *vista*: dibuja los datos, gestiona selección y scroll |
| `RowActionDelegate` | La *celda*: cómo se pinta y cómo reacciona a eventos una columna concreta |

La vista no sabe nada de SQL: pregunta al modelo. El modelo no sabe dibujar: responde con datos y flags.

**`StudentTableModel`** extiende `QSqlTableModel` y añade una **columna virtual**:

```cpp
int StudentTableModel::columnCount(...) const {
  // Columna virtual de acciones al final; el SQL de la tabla no cambia.
  return QSqlTableModel::columnCount(parent) + 1;
}
```

- `columnCount()` devuelve las columnas de la tabla **más una**: la columna "Acciones" (índice 8) **no existe en SQLite**. ¿Por qué funciona? Porque la tabla solo se *consulta* (`select()`): no hay INSERT/UPDATE que deba incluir esa columna.
- `data()`: para la columna virtual devuelve `QVariant()` (vacío — quién la pinta es el delegado). Para la columna 7 (`calificacion`), si el valor es `NULL` muestra el texto **"Sin calificar"**; si no, el número con un decimal (`QString::number(valor.toDouble(), 'f', 1)`).
- `headerData()`: el encabezado de esa columna es `"Acciones"` (solo presentación).
- `flags()`: la columna virtual es `ItemIsEnabled | ItemIsSelectable` — no editable (el clic lo maneja el delegado), pero seleccionable para que la fila entera se seleccione.

**La configuración de la vista** (constructor de `MainWindow`): `setSelectionBehavior(SelectRows)` + `setSelectionMode(SingleSelection)` para selección por fila única; `setEditTriggers(NoEditTriggers)` — la edición **nunca es in-place**, siempre pasa por los diálogos; filas con color alternado; header vertical oculto. El `QHeaderView` reparte el ancho: columnas 1, 2, 3 y 6 en `Stretch` (estiran), 0 y 7 en `ResizeToContents` (ancho natural), 4 y 5 en `Interactive` con anchos iniciales de 110 y 80, y la columna 8 (Acciones) en `ResizeToContents` — su ancho lo fija el `sizeHint()` del delegado.

**El flujo `refrescarTabla()`**: llama a `m_model->select()` (reejecuta la consulta y recarga el modelo) y muestra `"Estudiantes: N"` en la barra de estado. Se invoca al arrancar la ventana y después de cada alta/edición aceptada, además del botón Refrescar. Nota: no se re-escalan las columnas al refrescar para no anular el reparto del header.

**Un solo camino de edición**: `editarFila(const QModelIndex&)` toma `m_model->record(index.row())`, pasa los 8 campos a `StudentDialog::setEditData()` y, si el diálogo se acepta, refresca. El botón global Editar, el doble clic y el botón de fila **convergen en esta misma ranura**: menos código duplicado, comportamiento idéntico.

---

## 7. Botón por fila (rowactiondelegate.h/.cpp)

**Respuesta corta: en vez de meter un `QPushButton` real por fila, la columna "Acciones" la pinta un delegado, y solo en la fila seleccionada.** Menos widgets, modelo intacto, cero ruido visual.

- **`paint()`**: primero delega el fondo a `QStyledItemDelegate::paint` (fondo normal/de selección). Si `option.state` **no** trae `QStyle::State_Selected`, la celda queda vacía: sin botón. Si la fila está seleccionada, construye un `QStyleOptionButton` y llama a `style->drawControl(QStyle::CE_PushButton)` — el botón lo dibuja **el estilo del sistema**, no código propio de dibujado.
- **`editorEvent()`**: detecta el clic. Exige: `MouseButtonRelease` (no press — para no interferir con la selección ni con el doble clic), botón izquierdo, índice válido, y que el punto del clic caiga **dentro de `botonRect(option.rect)`** — la misma geometría que usó `paint()`, compartida a propósito para que la zona de clic coincida exactamente con lo dibujado. Además consulta `selectionModel()->isRowSelected(...)`: la vista no entrega `State_Selected` en `editorEvent`, y esa comprobación protege el caso de que la selección se haya movido entre el repintado y el clic. Si todo cuadra, `emit editRequested(index)` y `return true` (evento consumido); si no, `return false` y el evento sigue su curso normal (selección, doble clic...).
- **`sizeHint()`**: ancho mínimo = texto "Editar" + paddings + márgenes (`kMargenBoton = 4`, `kPaddingTextoBoton = 20`). `ResizeToContents` de la columna 8 lo usa para dejar la columna compacta sin recortar el botón.
- **La señal propia**: el delegado declara `signals: void editRequested(const QModelIndex&)` — esto requiere `Q_OBJECT` y por eso el header del delegado lo lleva. `MainWindow` conecta esa señal a `editarFila`, la misma ranura del doble clic y del botón global Editar.

**¿Por qué delegado y no botones reales?** Con N filas tendrías N widgets vivos, cada uno con su ciclo de vida, su repintado y su gestión de memoria. Además, un botón real invitaría a edición in-place del modelo, rompiendo el flujo "siempre por diálogo". El delegado dibuja solo la celda visible (on-demand) y deja el modelo limpio. Como ventaja extra, el patrón "el botón aparece solo en la fila seleccionada" es gratis: `paint` lo decide con una condición. Cuando la selección cambia, el `currentRowChanged` fuerza `update()` de las celdas anterior y actual, y el botón "se mueve" de fila.

---

## 8. Alta/edición de estudiantes (studentdialog.h/.cpp)

**Un diálogo, dos modos, decidido por `m_id`**: `int m_id = -1` por defecto (alta); `setEditData()` lo cambia a `id >= 1` (edición) y rellena los campos. En edición, además, cambia el título a "Editar estudiante", enfoca el nombre y lo selecciona (`selectAll()`) para sobrescribir rápido.

**Validaciones en `guardar()`, citadas del código**:

- Campos obligatorios: `nombre`, `apellido` y `cedula` se recogen con `.trimmed()`. Si alguno está vacío: `QMessageBox::warning` + **foco en el campo faltante** (`setFocus()`). La UI nunca deja llegar un NULL a la BD.
- Cédula duplicada (studentdialog.cpp:81):

```cpp
dup.prepare("SELECT id FROM estudiantes WHERE cedula = :c AND id != :id");
dup.bindValue(":c", cedula);
dup.bindValue(":id", m_id); // -1 en modo agregar: nunca coincide
```

  El truco está en `id != :id`: en alta `m_id = -1` y ninguna fila tiene id -1 (no molesta); en edición excluye la fila propia, permitiendo guardar sin cambios. La `UNIQUE` de la BD es la red de seguridad final: `mostrarErrorEscritura()` detecta `"UNIQUE"` en el texto del error y muestra el mensaje amigable de cédula duplicada.

- Calificación: si `checkSinCalificar` está marcado (por defecto, y deshabilita el spin vía `on_checkSinCalificar_toggled`), el valor que se guarda es `QVariant()` → **NULL** en la BD. Desmarcado usa `spinCalificacion` (rango efectivo 0–20: máximo 20 fijado en el `.ui`, mínimo 0 por defecto del widget, 2 decimales). En edición, `calificacion.isNull()` vuelve a marcar el check.
- Combos con listas fijas: trayecto I–IV, tramo 1–2, sección A–E, definidos como ítems estáticos en el `.ui` — el usuario no escribe texto libre, no hay valores inválidos. Detalle de orden: `setEditData` carga el trayecto **antes** que el tramo (y el código lo comenta) porque `on_comboBoxTrayecto_currentIndexChanged` reinicia el tramo a "1" (`setCurrentIndex(0)`) al cambiar de trayecto; si el orden fuera inverso, el tramo cargado se perdería.

**Escritura**: `m_id < 0` → `INSERT INTO estudiantes (nombre, apellido, cedula, trayecto, tramo, seccion, calificacion) VALUES (:nombre, ...)`. `m_id >= 1` → `UPDATE ... WHERE id = :id`. Ambas 100 % preparadas. `on_btnGuardar_clicked` llama a `guardar()` y, si retorna `true`, `accept()`; entonces `MainWindow` refresca la tabla.

---

## 9. Gestión de administradores (admindialog.h/.cpp)

**Un diálogo con dos caras**. El mismo botón `btnAgregar` dice **"Agregar"** en modo alta y **"Guardar"** en modo edición: `iniciarModoEdicion()` cambia `setText("Guardar")`, muestra `btnCancelarEdicion` y cambia el placeholder de la contraseña a "Nueva contraseña" (pista visual de que vacía = conservar). `on_btnAgregar_clicked` despacha: si `m_idEdicion >= 1`, va a `guardarEdicion()`; si no, hace la alta.

- **Alta**: usuario y contraseña obligatorios (aviso + foco). Duplicado previo con `SELECT id FROM usuarios WHERE usuario = :u`. INSERT con rol `'admin'` y `hashPassword(clave)`. El Enter dentro de los campos de texto llama al mismo slot (conexión manual con `returnPressed` — buen ejemplo de la sintaxis moderna de `connect`).
- **Edición**: `iniciarModoEdicion()` se niega a entrar si el rol es `principal`. `guardarEdicion()` construye **dos SQL distintos** según haya contraseña nueva:

```cpp
// Sin contraseña: no se toca el campo password (se conserva la actual)
"UPDATE usuarios SET usuario = :u WHERE id = :id AND rol != 'principal'"
// Con contraseña: también se actualiza el hash
"UPDATE usuarios SET usuario = :u, password = :p WHERE id = :id AND rol != 'principal'"
```

  Ambos incluyen `AND rol != 'principal'` — la segunda capa del blindaje — y el duplicado en edición excluye el id propio (`WHERE usuario = :u AND id != :id`).
- **El chequeo que evita el éxito falso**: después del UPDATE, `upd.numRowsAffected() == 0` significa que ninguna fila cambió (era el principal o el id ya no existe). Sin ese chequeo, un UPDATE que no toca filas "tendría éxito" en silencio. El código avisa y abandona el modo edición.
- **Eliminar**: confirmación con `QMessageBox::question` (por defecto `No`), y luego:

```cpp
del.prepare("DELETE FROM usuarios WHERE id = :id AND rol != 'principal'");
```

**¿Por qué el blindaje en BD es el que de verdad protege?** La UI deshabilita botones y avisa (capas de presentación), pero la UI es solo una fachada: cualquier otro camino hasta la BD — una consulta externa, un bug, un futuro código que olvide la UI — no pasa por los botones. La capa **SQL** rechaza el UPDATE/DELETE aunque alguien lo intente saltándose la interfaz. Por eso el proyecto aplica las dos capas y no una sola, y por eso el comentario del header insiste en que el principal está protegido "en UI, en el UPDATE y en el DELETE".

---

## 10. Reglas de negocio

| Regla | Implementación (dónde) |
|---|---|
| Cédula sin duplicados | Consulta previa `WHERE cedula = :c AND id != :id` (studentdialog.cpp:81) + `UNIQUE` en la tabla |
| Sin campos vacíos | Validación en `guardar()` con aviso y foco + `NOT NULL` en la BD |
| Solo la cédula es única | El resto de campos puede repetirse; `UNIQUE` solo en `cedula` (CREATE TABLE) |
| Los estudiantes no se eliminan | No existe ninguna sentencia `DELETE` para `estudiantes` en el proyecto |
| La calificación es editable | `setEditData` + `spinCalificacion` en modo edición; NULL = "Sin calificar" |
| Usuario admin sin duplicados | Consulta previa (alta y edición) + `UNIQUE` en `usuarios.usuario` |
| El principal está blindado | UI deshabilita + `UPDATE`/`DELETE` con `rol != 'principal'` + chequeo de `numRowsAffected` |
| Contraseña vacía al editar = conservar | El UPDATE se construye sin columna `password` (admindialog.cpp:205–211) |
| BD persistente | Archivo en `AppDataLocation` + sentencias idempotentes; se autorecrea si se borra |

---

## 11. Seguridad (aprendizaje)

- **Consultas preparadas en todo el proyecto**: login, chequeos de duplicado, seed, INSERTs, UPDATEs y DELETEs usan `prepare()` + `bindValue()`. Ningún dato del usuario se concatena a SQL. Es la lección de seguridad más importante que deja el código.
- **Hash sin salt**: SHA-256 hex es correcto para aprender el patrón (guardar derivado, nunca en claro, comparar derivado) pero **no es suficiente para producción**: sin salt, dos usuarios con la misma contraseña tienen el mismo hash y las tablas rainbow aceleran la reversión. Nota educativa: en un sistema real hay que migrar a PBKDF2 (`QPasswordDigestor::deriveKey` en Qt) o Argon2/bcrypt, con salt aleatorio por usuario. El propio README ya hace esta advertencia.
- **Blindaje doble** del principal: presentación (botones deshabilitados, avisos) + base de datos (`rol != 'principal'` en UPDATE y DELETE). La BD es la última línea y la que decide de verdad.
- Límite a conocer: el archivo SQLite se guarda en claro en el directorio de datos del usuario y el driver es el `QSQLITE` estándar; si algún día hubiera datos sensibles habría que cifrarlo (p. ej. SQLCipher), no "esconderlo" en AppDataLocation.

---

## 12. "Así se hace X" — guiones end-to-end

### (a) Arrancar la aplicación

| Paso | Componente | Qué ocurre |
|---|---|---|
| 1 | `main()` → `initDatabase()` | Abre/crea/migra/siembra `crudqt.db`. Si falla, `return 1` |
| 2 | `LoginDialog login; login.exec()` | Diálogo modal. `on_btnIngresar_clicked` → `validarCredenciales()` (SQL preparado + hash) |
| 3 | `login.exec() != QDialog::Accepted` | Cancelar o cerrar → `return 0` (la app muere sin CRUD) |
| 4 | `MainWindow w; w.show()` | Constructor: modelo `estudiantes`, vista, delegado, headers; `refrescarTabla()` al final |
| 5 | `a.exec()` | Bucle de eventos: la app queda viva hasta cerrar la ventana |

### (b) Agregar un estudiante

| Paso | Componente | Qué ocurre |
|---|---|---|
| 1 | `MainWindow::on_btnAgregar_clicked` | Crea `StudentDialog` (constructor; `m_id = -1`) |
| 2 | `StudentDialog::guardar()` | Valida nombre/apellido/cédula; chequea cédula duplicada (`id != :id` con -1); prepara INSERT |
| 3 | `on_btnGuardar_clicked` → `accept()` | Si `guardar()` da true, cierra con `Accepted` |
| 4 | `MainWindow::refrescarTabla()` | `m_model->select()` recarga; `statusBar` muestra el nuevo total |

### (c) Editar un estudiante desde el botón de fila

| Paso | Componente | Qué ocurre |
|---|---|---|
| 1 | `RowActionDelegate::paint` | Solo con `State_Selected` dibuja el botón "Editar" con el estilo del sistema |
| 2 | `RowActionDelegate::editorEvent` | Release + botón izquierdo + clic dentro de `botonRect` + fila seleccionada → `emit editRequested(index)` |
| 3 | `MainWindow::editarFila` | `m_model->record(row)` → `StudentDialog::setEditData(...)` (modo edición, `m_id` real) |
| 4 | `StudentDialog::guardar()` | Chequea duplicado excluyendo la fila propia; UPDATE preparado con `WHERE id = :id` |
| 5 | `MainWindow::refrescarTabla()` | Recarga y muestra el total |

### (d) Agregar / editar / eliminar un administrador

| Acción | Componente | Qué ocurre |
|---|---|---|
| Agregar | `AdminDialog::on_btnAgregar_clicked` (alta) | Valida campos, chequea usuario duplicado, INSERT con rol `'admin'` + `hashPassword`; `refrescarLista()` |
| Editar | `on_btnEditar_clicked` → `iniciarModoEdicion` → botón "Guardar" → `guardarEdicion()` | No entra si es `principal`; el SQL lleva `rol != 'principal'`; contraseña vacía conserva la actual; `numRowsAffected()==0` → aviso y salida del modo |
| Eliminar | `on_btnEliminar_clicked` | Bloqueo en UI si es `principal`; confirmación Yes/No; `DELETE ... WHERE id = :id AND rol != 'principal'`; `refrescarLista()` |

### (e) ¿Qué pasa si la base de datos desaparece?

| Paso | Componente | Qué ocurre |
|---|---|---|
| 1 | Se borra `crudqt.db` (con la app cerrada) | El código no cambia: no se "repara" nada en caliente, solo se regenera al arrancar |
| 2 | `main()` → `initDatabase()` | `mkpath` recrea la carpeta; `CREATE TABLE IF NOT EXISTS` recrea las dos tablas vacías; `INSERT OR IGNORE` siembra de nuevo `admin/admin` con rol `principal` |
| 3 | Login `admin/admin` | El hash del semilla vuelve a coincidir; el sistema funciona como recién instalado |

---

## 13. Checklist para el estudiante (autoevaluación)

Antes de dar el proyecto por entendido, intenta responder sin mirar el código:

- [ ] ¿Qué pasaría si en `validarCredenciales` quito `bindValue` y concateno el texto del usuario al SQL? ¿Por qué `' OR '1'='1` sería peligroso y por qué ahora no lo es?
- [ ] ¿Por qué la columna "Acciones" no existe en la base de datos? ¿Qué haría falta tocar si mañana quisieras que también fuera persistente?
- [ ] ¿Qué hace uic con `logindialog.ui`? ¿Dónde acaba `setupUi()` y qué llama internamente que activa las auto-conexiones?
- [ ] ¿Qué genera moc para `RowActionDelegate`? ¿Por qué su señal `editRequested` no podría existir sin `Q_OBJECT`?
- [ ] ¿Por qué el login compara `hashPassword(password)` y no el texto de la contraseña?
- [ ] ¿Por qué en la consulta de cédula duplicada hay `id != :id`, y qué valor toma `:id` al agregar? ¿Qué pasaría si ese chequeo no existiera y solo confiáramos en el UNIQUE?
- [ ] ¿Por qué `setEditData` carga el trayecto antes que el tramo? ¿Qué rompería el orden inverso?
- [ ] Si edito un administrador y dejo la contraseña vacía, ¿qué SQL exacto se ejecuta y por qué no se borra la contraseña?
- [ ] `numRowsAffected() == 0` después del UPDATE de un admin: ¿qué escenario detecta y por qué es necesario?
- [ ] El botón de fila desaparece de la fila anterior al cambiar la selección: ¿qué función de `MainWindow` repinta esas celdas y qué señal la dispara?
- [ ] ¿Por qué el blindaje del principal necesita la capa SQL si la UI ya deshabilita los botones?
- [ ] ¿Por qué `initDatabase()` puede ejecutarse mil veces sin duplicar el admin? Nombra las tres construcciones SQL que lo garantizan.
- [ ] Si borras `crudqt.db` mientras la app está abierta y sigues usando la tabla: ¿qué crees que ocurre y por qué la regeneración solo pasa en el arranque? (Pista: `initDatabase()` se llama una sola vez, en `main`.)