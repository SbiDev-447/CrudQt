# Cómo armar tu DB en SQLite — Guía de estudio con el esquema real de CrudQt

Esta guía explica SQLite usando la base de datos de CrudQt como ejemplo trabajado. No es un tutorial genérico: el esquema completo y todas las sentencias de la [sección 7](#7-las-consultas-que-la-app-ejecuta-de-verdad) están copiados de `database.cpp`, `logindialog.cpp`, `studentdialog.cpp` y `admindialog.cpp`. Si lees con el código abierto al lado, cada afirmación se puede verificar. Hay tres bloques que **no** salen del proyecto y que se distinguen aquí mismo: la tabla que contrasta `AUTOINCREMENT` en la [sección 3.1](#31-usuarios--databasecpp55-59), los experimentos sobre bases desechables de la [sección 6.3](#63-por-qué-el-default-admin-no-es-decorativo) y los `INSERT` de prueba de la [sección 8.2](#82-con-la-cli-sqlite3), que escriben siempre sobre una copia.

**Ver también:** [Explication.md](Explication.md) — cómo está construido todo el sistema Qt/C++ · [forWindowsBuilt.md](forWindowsBuilt.md) — compilación cruzada y empaquetado.

---

## Índice

| Sección | Qué responde |
|---|---|
| [1](#1-qué-es-sqlite-y-por-qué-encaja-en-una-app-de-escritorio) | Por qué una app de escritorio usa SQLite y no un servidor de base de datos |
| [2](#2-las-tres-cosas-que-hay-que-saber) | Tablas, tipos y el archivo: el modelo mental mínimo |
| [3](#3-el-esquema-real-de-crudqt-columna-por-columna) | `usuarios` y `estudiantes` columna por columna, con el porqué de cada decisión |
| [4](#4-la-distinción-que-el-proyecto-depende-text-not-null-vs-real-null) | Por qué `REAL NULL` produce la etiqueta "Sin calificar" |
| [5](#5-initdatabase-paso-a-paso) | El arranque real de la base de datos, en el orden en que se ejecuta |
| [6](#6-la-migración-con-pragma-table_info) | Qué es una migración y cómo la resuelve este proyecto |
| [7](#7-las-consultas-que-la-app-ejecuta-de-verdad) | Login, CRUD de estudiantes y CRUD de administradores |
| [8](#8-cómo-inspeccionar-la-base-de-datos-tú-mismo) | Abrir `crudqt.db` con la CLI o con Qt Creator |
| [9](#9-hashpassword-sha-256-sin-salt-con-honestidad) | Qué protege el hash y qué no protege |

---

## 1. Qué es SQLite y por qué encaja en una app de escritorio

**SQLite es una biblioteca, no un servidor.** Toda la base de datos —esquema, tablas, índices y datos— es **un solo archivo** en el disco. No hay servicio que arranque, ni puerto, ni usuario/contraseña de base de datos, ni configuración aparte. La diferencia con un motor cliente-servidor:

| | SQLite (CrudQt) | MySQL / PostgreSQL |
|---|---|---|
| Dónde vive el dato | Un archivo: `crudqt.db` | Un proceso `mysqld` / `postgresql` en otra máquina |
| Qué hay que instalar | Nada extra: es una biblioteca | Un servidor, su servicio, su usuario y su contraseña |
| Cómo se conecta | La ruta del archivo | Host, puerto, usuario, contraseña, socket o red |
| Cómo se hace backup | Copiar el archivo con la app cerrada | `mysqldump`, `pg_dump`, plan de restores |
| Cómo se instala la app | Un ejecutable y ya | El ejecutable **más** la BD que tiene que existir |
| Cuándo tiene sentido | Un archivo, un proceso, un usuario | Muchos usuarios, muchos procesos, datos compartidos |

Para una app de escritorio las cuatro primeras filas no son un detalle: son **el criterio de diseño entero**. Un gestor de estudiantes que guarda su BD en el home del usuario puede copiarse a un USB y funcionar en otra máquina sin instalar nada. Con un servidor, el mismo ejecutable en el USB no encontraría su base de datos y el usuario no sabría por qué.

En Qt esto se conecta en tres piezas, todas reales en este proyecto:

| Pieza | Dónde | Qué es |
|---|---|---|
| Módulo `Sql` | `CMakeLists.txt`: `find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql)` | Sin este módulo, `QSqlDatabase` ni siquiera compila |
| Driver `QSQLITE` | `database.cpp:42`: `QSqlDatabase::addDatabase("QSQLITE")` | El plugin que traduce las llamadas de Qt al motor embebido |
| Archivo | `database.cpp:38`: `rutaDb = dirDatos + "/crudqt.db"` | El único artefacto que hay que respaldar |

> El driver es un **plugin que se carga en tiempo de ejecución**, no está enlazado en el binario. Por eso el zip de Windows incluye `plugins/sqldrivers/qsqlite.dll`. Ojo con la etiqueta: en esa tabla la palabra **Imprescindible** es la de `plugins/platforms/qwindows.dll`; `qsqlite.dll` figura como «Driver de SQLite; sin él no hay base de datos». Y el efecto real de que falte no es una app que arranca sin datos, sino una app que **no llega a abrirse**: sin el driver, `db.open()` falla, `initDatabase()` devuelve `false` y `main.cpp:18-20` hace `return 1`. El proceso termina con código 1 **antes de mostrar ninguna ventana** —tampoco el login—. Comprobado contra Qt 6.8.2. Está documentado en [forWindowsBuilt.md](forWindowsBuilt.md#contenido-del-zip).

---

## 2. Las tres cosas que hay que saber

### 2.1 El archivo

Un archivo de SQLite se puede abrir con cualquier herramienta que hable el formato, sin que esté ninguna aplicación abierta. Eso convierte la base de datos en un artefacto inspeccionable, y de paso en un riesgo: quien tenga acceso de lectura al archivo tiene todos los datos.

| Propiedad | Valor real en CrudQt | Cómo se verificó |
|---|---|---|
| Ruta del archivo | `<AppDataLocation>/crudqt.db` | `database.cpp:36-38` |
| Modo de journal | `delete` (el valor por defecto; no se cambia en el proyecto) | `PRAGMA journal_mode` sobre el archivo real |
| Tablas internas que añade el motor | `sqlite_sequence`, creada por `AUTOINCREMENT` | `.schema` sobre el archivo real |
| Cifrado | Ninguno | El driver es el `QSQLITE` estándar |

### 2.2 Las tablas

Una tabla se crea con `CREATE TABLE`. Este proyecto **solo** usa dos sentencias de ese tipo, y las dos son idénticas en su forma:

```sql
CREATE TABLE IF NOT EXISTS usuarios (...);
CREATE TABLE IF NOT EXISTS estudiantes (...);
```

`IF NOT EXISTS` es la pieza que hace que todo el arranque sea idempotente: si la tabla ya está, la sentencia **no hace nada y no da error**. Sin ella, el segundo arranque de la app fallaría con "table usuarios already exists".

> **Lo que `IF NOT EXISTS` no hace:** no actualiza una tabla que ya existe. Si mañana añades una columna al `CREATE TABLE` de `usuarios`, una base de datos ya creada **no** la recibe. Ese caso exacto es el que resuelve la migración de la [sección 6](#6-la-migración-con-pragma-table_info), y es la razón por la que existe una función aparte.

### 2.3 Los tipos

SQLite es de tipado dinámico: el tipo declarado no restringe qué se puede guardar, sino **cómo se interpreta** lo que se guarda. Esa interpretación se llama *afinidad*.

| Afinidad | Se activa si el tipo declarado contiene… | Ejemplo en el proyecto | Qué se guarda |
|---|---|---|---|
| `INTEGER` | `INT` | `id INTEGER` | Números enteros; también `TRUE`/`FALSE` |
| `TEXT` | `CHAR`, `CLOB` o `TEXT` | `nombre TEXT`, `creado_en TEXT` | Cadenas de texto |
| `REAL` | `REAL`, `FLOA` o `DOUB` | `calificacion REAL` | Números de coma flotante |
| `NUMERIC` | cualquier otra cosa | — | Números, con conversión si hace falta |
| `BLOB` | `BLOB`, o el tipo no se declara | — | Bytes sin interpretar |

CrudQt usa `INTEGER`, `TEXT` y `REAL`. Las consecuencias prácticas:

- **`INTEGER PRIMARY KEY` es un alias del `rowid`.** La clave primaria entera no crea una tabla ni una búsqueda aparte: *es* la fila. Por eso en SQLite una PK entera es lo más rápido que hay.
- **`TEXT` no significa "texto de 10 caracteres".** No hay límites de longitud: un `TEXT` de un millón de caracteres es igual de válido.
- **No existe un tipo fecha.** Las fechas se guardan como `TEXT` con un formato fijo —`datetime('now')` produce `YYYY-MM-DD HH:MM:SS`— y ese formato **se ordena lexicográficamente igual que cronológicamente**, siempre que todas las fechas usen el mismo formato. Por eso `creado_en` es `TEXT` y no un tipo inventado.

`PRAGMA table_info(estudiantes)` sobre el archivo real devuelve las nueve columnas con su afinidad, su `notnull`, su valor por defecto y si es clave primaria:

```
cid  name           type     notnull  dflt_value       pk
---  -------------  -------  -------  ----------------  --
0    id             INTEGER  0                         1
1    nombre         TEXT     1                         0
2    apellido       TEXT     1                         0
3    cedula         TEXT     1                         0
4    trayecto       TEXT     1                         0
5    tramo          TEXT     1                         0
6    seccion        TEXT     1                         0
7    calificacion   REAL     0                         0
8    creado_en      TEXT     0        datetime('now')  0
```

Lee las dos filas que surprisean: `id` tiene `notnull = 0` aunque sea clave primaria — SQLite lo hace NOT NULL implícitamente, pero no lo registra en la tabla. Y `calificacion` tiene `notnull = 0` porque **no lleva `NOT NULL`**: esa es la columna de la que habla la [sección 4](#4-la-distinción-que-el-proyecto-depende-text-not-null-vs-real-null).

---

## 3. El esquema real de CrudQt, columna por columna

Las dos sentencias, tal cual están en `database.cpp` (las líneas de cadena se unen para que se lean; los literales son idénticos).

### 3.1 `usuarios` — `database.cpp:55-59`

```sql
CREATE TABLE IF NOT EXISTS usuarios (
    id       INTEGER PRIMARY KEY AUTOINCREMENT,
    usuario  TEXT    NOT NULL UNIQUE,
    password TEXT    NOT NULL,
    rol      TEXT    NOT NULL DEFAULT 'admin'
)
```

| Columna | Restricciones | Por qué está así |
|---|---|---|
| `id` | `INTEGER PRIMARY KEY AUTOINCREMENT` | La identidad estable de la fila. `INTEGER` + `PRIMARY KEY` la convierten en el `rowid`, así que no hay índice adicional. `AUTOINCREMENT` garantiza que el identificador **nunca se reutiliza** (ver más abajo). |
| `usuario` | `TEXT NOT NULL UNIQUE` | Es la identidad de acceso. `UNIQUE` hace imposible tener dos administradores con el mismo nombre, y crea un índice automático (`sqlite_autoindex_usuarios_1`). `NOT NULL` impide el `NULL` —una fila sin usuario no identifica a nadie—, **no** el `''`: el usuario vacío lo rechaza la aplicación, en `admindialog.cpp:80-84`. |
| `password` | `TEXT NOT NULL` | Guarda el **hash** SHA-256 en hexadecimal, nunca el texto. `NOT NULL` impide el `NULL`, que sería una fila muerta sin hash, pero no impide `''`; la contraseña vacía la rechaza `admindialog.cpp:85-90`. |
| `rol` | `TEXT NOT NULL DEFAULT 'admin'` | Dos valores: `principal` y `admin`. El `DEFAULT 'admin'` es lo que recibe todo usuario que ya existía cuando se migró la columna. Aquí `NOT NULL` **sí** es lo que cierra el blindaje `rol != 'principal'`, y el mecanismo es contraintuitivo: `NULL != 'principal'` evalúa a `NULL`, que el `WHERE` no toma, así que un rol nulo **no** se cuela. Un rol **vacío** sí se cuela: `'' != 'principal'` da `1` y la fila pasa como un admin cualquiera. |

**Qué compra `AUTOINCREMENT` y qué cuesta.** Sin ella, SQLite asigna `max(id) + 1`: si borras la fila con el id más alto, el siguiente `INSERT` **reutiliza** ese número. Con ella, el número nunca vuelve. Comprobado sobre el motor real:

| Tabla | Filas | Se borra la de `id` más alto | Siguiente `INSERT` |
|---|---|---|---|
| `INTEGER PRIMARY KEY` | `1:x`, `2:y` | `2:y` | `2:z` — **reutiliza el 2** |
| `INTEGER PRIMARY KEY AUTOINCREMENT` | `1:x`, `2:y` | `2:y` | `3:z` — **sigue adelante** |

En CrudQt importa porque `id` es la identidad que la aplicación transporta: `StudentTableModel` ordena por `id` para que las filas no se reordenen al editar, y `AdminDialog` guarda el `id` en `Qt::UserRole` para borrar la fila correcta. Un identificador reutilizado haría que un registro borrado reapareciera confundido con otro.

**El coste:** `AUTOINCREMENT` obliga a SQLite a mantener la tabla interna `sqlite_sequence` y a actualizar un contador en cada `INSERT`. Es un coste real, y en una tabla donde los ids se reutilizan sin consecuencias valdría la pena no pagarlo. Aquí el proyecto elige no reutilizarlos.

### 3.2 `estudiantes` — `database.cpp:70-79`

```sql
CREATE TABLE IF NOT EXISTS estudiantes (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    nombre       TEXT    NOT NULL,
    apellido     TEXT    NOT NULL,
    cedula       TEXT    NOT NULL UNIQUE,
    trayecto     TEXT    NOT NULL,
    tramo        TEXT    NOT NULL,
    seccion      TEXT    NOT NULL,
    calificacion REAL    NULL,
    creado_en    TEXT    DEFAULT (datetime('now'))
)
```

| Columna | Restricciones | Por qué está así |
|---|---|---|
| `id` | `INTEGER PRIMARY KEY AUTOINCREMENT` | Igual que en `usuarios`: identidad estable y ordenable. |
| `nombre`, `apellido` | `TEXT NOT NULL` | Sin ellos la fila no identifica a nadie. `NOT NULL` cierra la puerta a que un `INSERT` futuro olvide un campo. |
| `cedula` | `TEXT NOT NULL UNIQUE` | La **única** columna única de la tabla: la cédula es el documento de identidad y no puede repetirse. |
| `trayecto`, `tramo`, `seccion` | `TEXT NOT NULL` | Son valores de listas fijas (trayecto I–IV, tramo 1–2, sección A–E), no texto libre. `TEXT` y no un número: `"I"` no es un número, y `2` no distingue el tramo 2 del trayecto 2. |
| `calificacion` | `REAL NULL` | La única columna que admite `NULL`. Es lo que permite que exista un estudiante sin nota todavía. |
| `creado_en` | `TEXT DEFAULT (datetime('now'))` | La rellena **el motor** en el momento del `INSERT`, no la aplicación. Por eso ningún `INSERT` del proyecto menciona esta columna. |

**Sobre `datetime('now')`.** Devuelve la hora actual en **UTC**, con el formato `YYYY-MM-DD HH:MM:SS`. Comprobado contra el reloj: el valor almacenado era `2026-09-25 00:29:27` mientras la hora local era `2026-09-25 20:29` — cuatro horas de diferencia, la zona del usuario. Guardar UTC tiene una ventaja concreta: dos máquinas en zonas horarias distintas escriben fechas ordenables entre sí, porque el formato nunca lleva sufijo.

**Sobre `REAL` y no `INTEGER`.** La escala es 0–20 con decimales, y hay notas como `18.5`. En la tabla real, `typeof(calificacion)` devuelve `real` para una nota y `null` para un estudiante sin nota. Con `INTEGER` la parte decimal se perdería; con `TEXT` no se podrían agregar ni promediar las notas sin conversiones.

### 3.3 Por qué `UNIQUE` solo en `cedula`

Esta es una decisión de negocio escrita en el esquema, y la razón es que **cualquier otra columna sí puede repetirse**: hay dos personas que se llaman igual, dos que comparten apellido, dos del mismo trayecto. La cédula es el documento que los distingue.

La consecuencia es que la base de datos no impone nada sobre los nombres: depende de la aplicación. Y es exactamente lo que dice la tabla de reglas de negocio del [README](../README.md):

| Dato | ¿Puede repetirse? | ¿Quién lo garantiza? |
|---|---|---|
| `cedula` | No | La BD: `UNIQUE`. La aplicación solo mejora el mensaje |
| `nombre`, `apellido` | Sí | Nada: es correcto que se repitan |
| `trayecto`, `tramo`, `seccion` | Sí | La UI: son combos con ítems fijos, no texto libre |

El coste de cada `UNIQUE` es un índice automático. Verificado en el archivo real: `PRAGMA index_list(estudiantes)` devuelve `sqlite_autoindex_estudiantes_1`, de origen `u` (constraint `UNIQUE`). Un índice por columna única es un precio bajo que se paga en cada `INSERT` y a cambio hace la búsqueda por cédula directa.

---

## 4. La distinción que el proyecto depende: `TEXT NOT NULL` vs `REAL NULL`

Esta es la parte del esquema que **no es un detalle de estilo**: de ella depende una etiqueta visible de la aplicación. La diferencia entre las dos columnas es una sola pregunta.

| | `nombre TEXT NOT NULL` | `calificacion REAL NULL` |
|---|---|---|
| La pregunta que responde | ¿Existe un nombre? | ¿Existe ya una nota? |
| La respuesta "no" | La BD **no** la rechaza: `''` es un `TEXT` válido | Es un valor: `NULL` |
| Cómo lo decide la UI | `nombre.isEmpty()` → aviso + foco | `checkSinCalificar` marcado |
| Qué llega a la BD | La cadena, siempre | Un número, o `NULL` |
| Cómo lo lee el modelo | `QSqlTableModel::data` directo | `StudentTableModel::data` lo intercepta |
| Qué ve el usuario | El nombre | `18.0`, o **"Sin calificar"** |

**`NOT NULL` no significa "no vacío".** Es la confusión más común con esta restricción, y en este proyecto se ve con total claridad. Comprobado sobre el motor real:

```sql
CREATE TABLE t (nombre TEXT NOT NULL);
INSERT INTO t VALUES ('');    -- correcto, sale con código 0
INSERT INTO t VALUES (NULL);  -- Error: stepping, NOT NULL constraint failed: t.nombre (19)
```

`NOT NULL` rechaza `NULL`, que significa *no se pasó el dato*. No rechaza `''`, que es un `TEXT` perfectamente válido de longitud cero: la fila existe y el nombre está ahí, solo que vacío. La columna responde "¿se pasó el nombre?", no "¿el nombre tiene algo dentro?".

Por eso el nombre vacío se detiene en C++ y no en la base de datos: `studentdialog.cpp:63-67` hace `if (nombre.isEmpty())` y devuelve antes de que exista un `INSERT` que ejecutar. La cadena `''` nunca llega a SQLite. Esa es la lección más transferible de todo el esquema: **una regla que la base de datos no sabe expresar tiene que vivir en la capa de aplicación**, y en Qt esa capa es C++. SQLite no tiene ningún tipo «texto no vacío» ni ninguna constraint que lo compruebe —no hay forma de declararlo en el `CREATE TABLE`—, así que el esquema no puede ahorrar ese trabajo: tiene que hacerlo el código que escribe.

**`NULL` no es cadena vacía ni cero.** Es "este dato todavía no existe". La diferencia es observable:

| Valor | Significado | `typeof()` | Se muestra como |
|---|---|---|---|
| `NULL` | Sin calificar | `null` | "Sin calificar" |
| `0.0` | Calificado con cero | `real` | `0.0` |
| `18.5` | Calificado con 18,5 | `real` | `18.5` |

Un estudiante con un 0 y un estudiante sin nota son **situaciones distintas**, y por eso la columna es `REAL NULL` y no `REAL NOT NULL DEFAULT 0`. Con `NOT NULL` y valor por defecto, los dos casos acabarían en el mismo sitio: el 0 se mostraría como "Sin calificar" y se perdería información real de una calificación.

**La cadena completa, de la UI a la pantalla.** Son cuatro eslabones y todos están en el código:

**1. Al guardar** (`studentdialog.cpp:97-101`) — un `QVariant` vacío se convierte en `NULL`:

```cpp
// Calificación: NULL cuando se marca "Sin calificar".
QVariant calificacion;
if (!ui->checkSinCalificar->isChecked()) {
  calificacion = ui->spinCalificacion->value();
}
```

Un `QVariant` por defecto está *null*, y `bindValue(":calificacion", calificacion)` liga ese `QVariant` nulo: SQLite recibe `NULL`. Si la casilla está desmarcada, se liga el `double` del spin.

**2. Al reabrir la edición** (`studentdialog.cpp:47-52`) — el `NULL` vuelve a ser la casilla marcada:

```cpp
if (calificacion.isNull()) {
  ui->checkSinCalificar->setChecked(true);
} else {
  ui->checkSinCalificar->setChecked(false);
  ui->spinCalificacion->setValue(calificacion.toDouble());
}
```

Sin este `if/else`, al editar un estudiante sin nota el `spin` mostraría `0.00` y el próximo guardado escribiría un cero falso.

**3. Al pintar** (`mainwindow.cpp:28-36`) — el modelo traduce almacenamiento a presentación:

```cpp
// Columna de calificación (índice 7): NULL se muestra como "Sin calificar".
if (index.column() == 7) {
  const QVariant valor = QSqlTableModel::data(index, Qt::EditRole);
  if (role == Qt::DisplayRole) {
    if (valor.isNull()) {
      return QStringLiteral("Sin calificar");
    }
    return QString::number(valor.toDouble(), 'f', 1);
  }
```

`QString::number(valor.toDouble(), 'f', 1)` formatea con un decimal fijo: por eso una nota guardada como `18.0` se ve `18.0` y no `18`.

**4. Al atenuar el texto** (`mainwindow.cpp:42-44`) — además del texto, el color:

```cpp
if (role == Qt::ForegroundRole && valor.isNull()) {
  return colorTextoSecundario();
}
```

**La lección de diseño que hay debajo:** "Sin calificar" es un *placeholder de presentación*, nunca un dato. La cadena `"Sin calificar"` no se escribe jamás en la base de datos: no aparece en ningún `INSERT`, no aparece en ningún `UPDATE` y no se puede encontrar con un `SELECT` sobre el archivo. La base de datos guarda la ausencia de nota; el modelo decide cómo se llama a esa ausencia en pantalla. Por eso se puede cambiar el texto, o el color, o el tema, sin tocar ni el esquema ni una sola sentencia SQL.

---

## 5. `initDatabase()` paso a paso

**Respuesta corta: `initDatabase()` es idempotente.** Se puede ejecutar mil veces y nunca duplica datos ni falla. Esa propiedad es la que permite que la base de datos sobreviva a cada arranque de la aplicación.

Este es el orden real de `database.cpp:33-106`. Cada paso indica qué construcción SQL lo hace seguro:

| # | Qué hace | Código | Por qué no se rompe al repetirlo |
|---|---|---|---|
| 1 | Resuelve la ruta del directorio de datos | `QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)` (línea 36) | Es una consulta al sistema operativo, no un efecto |
| 2 | Crea la carpeta si falta | `QDir().mkpath(dirDatos)` (línea 37) | `mkpath` no falla si la carpeta ya existe |
| 3 | Compone la ruta del archivo | `dirDatos + "/crudqt.db"` (línea 38) | Solo concatena cadenas |
| 4 | Registra el driver **una sola vez** | `if (!QSqlDatabase::contains(QSqlDatabase::defaultConnection)) QSqlDatabase::addDatabase("QSQLITE")` (41-43) | El guard evita el error "duplicate connection name" |
| 5 | Abre el archivo | `db.setDatabaseName(rutaDb); db.open()` (45-47) | `open()` sobre un archivo existente es una no-op |
| 6 | Crea `usuarios` **con `rol` incluido** | `CREATE TABLE IF NOT EXISTS usuarios (...)` (55-59) | `IF NOT EXISTS` |
| 7 | **Migra** la columna `rol` | `migrarColumnaRol()` (65) | El `PRAGMA` decide; ver [sección 6](#6-la-migración-con-pragma-table_info) |
| 8 | Crea `estudiantes` | `CREATE TABLE IF NOT EXISTS estudiantes (...)` (70-79) | `IF NOT EXISTS` |
| 9 | Siembra el admin principal | `INSERT OR IGNORE INTO usuarios (usuario, password, rol) VALUES (:u, :p, 'principal')` (87-88) | `OR IGNORE` + el `UNIQUE` de `usuario` |
| 10 | Asegura el rol del admin | `UPDATE usuarios SET rol = 'principal' WHERE usuario = 'admin'` (98-99) | Es un `UPDATE` a un valor fijo: ejecutarlo otra vez no cambia nada |
| 11 | Devuelve `true` | `return true` (105) | — |

**¿Por qué la migración está en el paso 7, entre las dos tablas?** Porque necesita que `usuarios` exista (paso 6) y debe completarse antes de que nada lea `rol` (pasos 9 y 10 la escriben y la filtran). Colocarla antes del paso 6 intentaría alterar una tabla inexistente; colocarla después del 9 dejaría el `INSERT` del semilla hablando con un esquema sin `rol`.

**Por qué el paso 6 ya incluye `rol` y aun así hay migración?** Porque el paso 6 solo define la columna en bases **nuevas**: si la tabla ya existe, `CREATE TABLE IF NOT EXISTS` no hace nada. Una base creada antes de que existiera el rol se queda sin él, y la migración del paso 7 la repara.

**El manejo de errores no es uniforme, y conviene decirlo.** Los pasos que ejecutan SQL **sí** comprueban su resultado y, si falla, escriben con `qCritical()` y devuelven `false`; `main()` convierte ese `false` en `return 1`. Los tres primeros pasos, no:

| Paso | Qué se descarta | Consecuencia |
|---|---|---|
| 1 | El resultado de `writableLocation()` (`database.cpp:36`) | Si el sistema operativo devolviera una ruta vacía, el código no se entera: seguiría con `"/crudqt.db"`, en la raíz del disco |
| 2 | El `bool` que devuelve `QDir().mkpath()` (`database.cpp:37`) | Si la carpeta no se puede crear, el fallo se descubre más tarde, en `db.open()` |
| 3 | El `QSqlDatabase` que devuelve `addDatabase()` (`database.cpp:42`) | Si el driver no existe, el fallo sale en `db.open()`, con un mensaje que habla de abrir y no de cargar el plugin |

Ninguna de esas tres omisiones es un bug en el arranque normal, y el paso 5 las absorbe: un `open()` fallido corta igual. Pero es un patrón que conviene reconocer, porque **descartar el retorno de una función que puede fallar no es lo mismo que comprobarlo**: el error existe, solo se detecta más tarde y con un mensaje que señala el síntoma en lugar de la causa. Aquí el síntoma es `open()` y la causa real es el driver ausente.

**¿Y si el archivo se borra?** En el próximo arranque los pasos 2, 6, 8 y 9 reconstruyen carpeta, tablas y admin desde cero, y el sistema vuelve a funcionar con `admin` / `admin`. La [sección 13 de Explication.md](Explication.md) tiene el guion completo de ese escenario.

---

## 6. La migración con PRAGMA table_info

### 6.1 El problema

La tabla `usuarios` se creó en una versión anterior del proyecto, **sin** la columna `rol`. Las cosas que hoy dependen de esa columna fallarían:

| Consulta que hoy existe | Qué pasaría en la base antigua |
|---|---|
| `SELECT id, usuario, rol FROM usuarios` (`admindialog.cpp:33`) | Error: no hay columna `rol` |
| `... AND rol != 'principal'` (`admindialog.cpp:206`, `296`) | Error: no hay columna `rol` |
| `UPDATE usuarios SET rol = 'principal' ...` (`database.cpp:99`) | Error: no hay columna `rol` |

Y el culpable no es un bug: es que **`CREATE TABLE IF NOT EXISTS` no modifica una tabla que ya existe**. Es la contrapartida del paso 6 de la sección anterior.

### 6.2 La solución: preguntar antes de actuar

`migrarColumnaRol()` (`database.cpp:14-29`) no adivina el estado de la base: **lo consulta**.

```cpp
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
```

`PRAGMA table_info(usuarios)` devuelve **una fila por columna** de la tabla. Sus columnas son `cid`, `name`, `type`, `notnull`, `dflt_value`, `pk` — y por eso el código mira `value(1)`: el índice 1 es `name`. Salida real sobre el archivo del proyecto:

```
cid  name      type     notnull  dflt_value  pk
---  --------  -------  -------  ----------  --
0    id        INTEGER  0                    1
1    usuario   TEXT     1                    0
2    password  TEXT     1                    0
3    rol       TEXT     1        'admin'     0
```

**Los dos caminos de la función:**

| Situación | Qué encuentra el `PRAGMA` | Qué hace la función | Resultado |
|---|---|---|---|
| La columna **ya existe** (base nueva, o ya migrada) | Una fila con `name = "rol"` | `return true` en la línea 18, sin ejecutar nada | Coste: una consulta de metadatos |
| La columna **no existe** (base antigua) | Ninguna fila con `name = "rol"`; el bucle termina | `ALTER TABLE usuarios ADD COLUMN rol TEXT NOT NULL DEFAULT 'admin'` (líneas 23-24) | La base queda al día |

Así la migración es **idempotente por construcción**: se puede llamar en cada arranque, mil veces, y solo altera algo la primera.

### 6.3 Por qué el `DEFAULT 'admin'` no es decorativo

`ADD COLUMN ... NOT NULL` **sin** `DEFAULT` es un error en SQLite cuando la tabla ya tiene filas. Comprobado sobre el motor real:

```
$ sqlite3 m2.db "CREATE TABLE u (id INTEGER PRIMARY KEY, n TEXT NOT NULL);
                 INSERT INTO u (n) VALUES ('a');
                 ALTER TABLE u ADD COLUMN rol TEXT NOT NULL;"
Error: stepping, Cannot add a NOT NULL column with default value NULL
```

El prefijo `stepping,` es el de la salida por lotes de `sqlite3`; escrito a mano en el prompt interactivo el mismo error sale como `Runtime error near line 3: Cannot add a NOT NULL column with default value NULL`. La causa es la misma en los dos casos: la tabla ya tiene filas, y SQLite no puede rellenar una columna `NOT NULL` nueva sin un valor con el que hacerlo.

Con el `DEFAULT 'admin'` funciona, y **todas las filas que ya existían reciben `admin`**:

```
id  usuario  password  rol
--  -------  --------  -----
1   a        h1        admin
2   b        h2        admin
```

Ese valor por defecto es la decisión de seguridad del proyecto: un administrador que existía antes del sistema de roles entra con el rol **menos privilegiado**, no como principal. El paso 10 de la [sección 5](#5-initdatabase-paso-a-paso) termina de arreglar el caso particular: el usuario `admin` siempre queda como `principal`.

### 6.4 Lo que este proyecto no hace

`PRAGMA user_version` — un contador de versión que permite decir "migración 3 de 5" — no aparece en el código, y es una decisión, no un descuido: con **una sola** migración, preguntar "¿existe esta columna?" es más barato y más legible que mantener un contador. Cuando las migraciones se multiplican y dependen entre sí, la forma escalable es un contador de versión; con una, un `PRAGMA` por arranque es la solución más simple que funciona.

---

## 7. Las consultas que la app ejecuta de verdad

**Once en esta sección, diecisiete en el proyecto.** Aquí están las once que ejecuta la UI, y ninguna más:

| Fichero | Qué aportan | Cuántas |
|---|---|---|
| `logindialog.cpp` | La validación del login | 1 |
| `studentdialog.cpp` | Cédula duplicada, `INSERT` de alta, `UPDATE` de edición | 3 |
| `admindialog.cpp` | Listado, dos consultas de duplicado, `INSERT` de alta, dos `UPDATE` de edición, `DELETE` | 7 |

Quedan seis más en `database.cpp`, que no aparecen en esta sección porque las ejecuta el arranque y no la interfaz: `PRAGMA table_info(usuarios)` (`:15`), `ALTER TABLE` (`:23-24`), los dos `CREATE TABLE` (`:55` y `:70`), el `INSERT OR IGNORE` de la semilla (`:87`) y el `UPDATE` que asegura el rol (`:98`).

**Once de las diecisiete usan consultas preparadas** (`prepare()` + `bindValue()`): las diez de los tres diálogos más el `INSERT` semilla de `database.cpp:87`. Las otras seis pasan SQL **literal** a `exec()` o al constructor de `QSqlQuery`: `admindialog.cpp:33` y las cuatro de `database.cpp:23,55,70,98`, más el `PRAGMA` de `database.cpp:15`. No lo necesitan porque no tienen nada que ligar: no hay ningún dato del usuario en ellas.

**Lo que sí se cumple en las diecisiete, y es lo que de verdad importa: ninguna concatena datos del usuario en una cadena SQL.** `exec()` solo recibe SQL literal, sin variables dentro; y donde hay datos, todos viajan como valores ligados tras `:`. Hasta el caso más sospechoso, el `UPDATE` de edición de `admindialog.cpp:205-211`, que construye una cadena en C++: no concatena nada, **elige entre dos constantes** según haya contraseña nueva, y el texto del usuario sigue ligarado en `:u` y `:p`. Por eso una entrada como `' OR '1'='1` se busca literalmente y no encuentra nada: en las diecisiete sentencias, el dato del usuario es siempre dato, nunca sintaxis.

### 7.1 Login — `logindialog.cpp:43-53`

```cpp
query.prepare("SELECT id FROM usuarios "
              "WHERE usuario = :u AND password = :p");
query.bindValue(":u", usuario);
query.bindValue(":p", hashPassword(password));
...
return query.next(); // true si encontró una fila
```

| Decisión | Por qué |
|---|---|
| `SELECT id` y no `SELECT *` | Solo hace falta saber si la fila existe. Devolver la fila completa sería desperdicio. |
| `password = :p` con `hashPassword(password)` | La base guarda el hash, así que hay que hashear la entrada con el mismo algoritmo para compararlas. |
| `prepare()` + `bindValue()` | El texto del usuario viaja como **dato**, nunca como SQL. Una entrada como `' OR '1'='1` se busca literalmente y no encuentra nada. |
| `return query.next()` | `next()` avanza al primer resultado: `true` significa que el login es válido. |
| No consulta `rol` | Cualquier usuario válido entra. El rol solo decide qué se puede hacer dentro de `AdminDialog`. |

### 7.2 Estudiantes — `studentdialog.cpp`

**Cédula duplicada, antes de escribir** (líneas 81-83):

```cpp
dup.prepare("SELECT id FROM estudiantes WHERE cedula = :c AND id != :id");
dup.bindValue(":c", cedula);
dup.bindValue(":id", m_id); // -1 en modo agregar: nunca coincide
```

`id != :id` es el detalle que hace que la misma consulta sirva para los dos modos: al agregar, `m_id` vale `-1` y no hay ninguna fila con ese id; al editar, excluye la fila propia y permite guardar sin cambios.

**Alta** (líneas 106-109) y **edición** (líneas 123-127):

```sql
INSERT INTO estudiantes (nombre, apellido, cedula, trayecto, tramo, seccion, calificacion)
VALUES (:nombre, :apellido, :cedula, :trayecto, :tramo, :seccion, :calificacion)

UPDATE estudiantes SET nombre = :nombre, apellido = :apellido, cedula = :cedula,
       trayecto = :trayecto, tramo = :tramo, seccion = :seccion,
       calificacion = :calificacion
WHERE id = :id
```

El `INSERT` **no menciona `id` ni `creado_en`**: el primero lo genera `AUTOINCREMENT` y el segundo lo rellena el motor con `datetime('now')`. Esos dos trabajos que la aplicación no hace son trabajo de la base de datos.

**La red final del `UNIQUE`** (`studentdialog.cpp:12-21`): si la consulta previa pasa pero otra conexión inserta la misma cédula a la vez, el `UNIQUE` la rechaza. El código detecta esa respuesta por el texto del error:

```cpp
if (error.text().contains("UNIQUE", Qt::CaseInsensitive)) {
  QMessageBox::warning(parent, "Cédula duplicada", ...);
  return;
}
```

El mensaje real del motor es `UNIQUE constraint failed: estudiantes.cedula`, que contiene `"UNIQUE"`. La UI y la BD se cubren: la UI da el mensaje amable, la BD impide el dato sucio.

### 7.3 Administradores — `admindialog.cpp`

| Operación | SQL (líneas) |
|---|---|
| Listar | `SELECT id, usuario, rol FROM usuarios ORDER BY id` (33) |
| Duplicado en alta | `SELECT id FROM usuarios WHERE usuario = :u` (94) |
| Alta | `INSERT INTO usuarios (usuario, password, rol) VALUES (:u, :p, 'admin')` (111-112) |
| Duplicado en edición | `SELECT id FROM usuarios WHERE usuario = :u AND id != :id` (188) |
| Edición sin contraseña | `UPDATE usuarios SET usuario = :u WHERE id = :id AND rol != 'principal'` (206-207) |
| Edición con contraseña | `UPDATE usuarios SET usuario = :u, password = :p WHERE id = :id AND rol != 'principal'` (209-210) |
| Eliminar | `DELETE FROM usuarios WHERE id = :id AND rol != 'principal'` (296) |

Cinco cosas que enseñan estas siete sentencias:

**El listado no pide la contraseña.** `SELECT id, usuario, rol` deja la columna fuera del resultado. El `id` que la UI necesita para borrar viaja escondido en `Qt::UserRole` (`admindialog.cpp:44`), no en una columna visible.

**El rol se fija en el SQL, no en la UI.** El alta escribe `'admin'` literal. El usuario no elige su propio rol, y aunque modificara el formulario, el valor de la base seguiría siendo el que dice la sentencia.

**La contraseña vacía no borra nada.** `guardarEdicion()` construye **dos** cadenas distintas según haya contraseña nueva: si está vacía, la sentencia no menciona la columna `password`, así que la fila conserva su hash. La UI lo anuncia cambiando el *placeholder* a "Nueva contraseña".

**El blindaje del principal está en el SQL, no en la interfaz.** `AND rol != 'principal'` aparece en el `UPDATE` y en el `DELETE`. La UI además deshabilita los botones, pero eso es fachada: esta capa es la que de verdad rechaza la operación, aunque el camino que llegue a la base de datos pase por otro sitio.

**Y una más, que es la trampa clásica del "UPDATE que no cambia nada":** un `UPDATE` que no encuentra filas **no da error**. Por eso el código comprueba `numRowsAffected()` (`admindialog.cpp:232`): si es `0`, ninguna fila cambió —porque era el principal o porque el id ya no existe—, y avisa en vez de fingir que guardó.

---

## 8. Cómo inspeccionar la base de datos tú mismo

### 8.1 La ruta del archivo

`database.cpp:35-38` la compone con `QStandardPaths::AppDataLocation`, que Qt resuelve según el sistema operativo:

| Sistema | Ruta del archivo |
|---|---|
| Linux | `~/.local/share/CrudQt/crudqt.db` → `/home/<usuario>/.local/share/CrudQt/crudqt.db` |
| Windows | `%APPDATA%\CrudQt\crudqt.db` → `C:\Users\<usuario>\AppData\Roaming\CrudQt\crudqt.db` |
| macOS | Qt lo resuelve bajo `~/Library/Application Support/` |

Dos detalles de esa ruta que confunden a quien la busca a mano:

- **La carpeta y el archivo no se llaman igual.** La carpeta es `CrudQt` con mayúscula, y viene del nombre de la aplicación —que Qt toma del ejecutable porque nadie llama a `setApplicationName()`. El archivo es `crudqt.db` en minúsculas porque está escrito a mano en la línea 38. En un sistema de archivos con distingue mayúsculas de minúsculas, buscar `CRUDQT.DB` no encuentra nada.
- **La app no pregunta dónde está.** No hay ruta configurable ni archivo de settings para la base de datos: la decide Qt. Para verla desde tu propio código, la llamada es exactamente la de `database.cpp:36`.

### 8.2 Con la CLI `sqlite3`

Es la forma más rápida y no necesita Qt. Con la aplicación **cerrada**:

```bash
# Abrir el archivo real
sqlite3 ~/.local/share/CrudQt/crudqt.db

# Dentro del prompt de sqlite3:
.schema                            # el CREATE TABLE real, tal como está
.tables                            # estudiantes, usuarios
SELECT id, usuario, rol FROM usuarios;
SELECT * FROM estudiantes;
PRAGMA table_info(estudiantes);     # columnas, tipos y restricciones
PRAGMA index_list(estudiantes);    # los índices que crea cada UNIQUE

# Y todo en una línea, sin entrar al prompt:
sqlite3 ~/.local/share/CrudQt/crudqt.db "SELECT * FROM estudiantes;"
```

La salida de `.schema` sobre el archivo real, para comparar con el [README](../README.md):

```sql
CREATE TABLE usuarios (id INTEGER PRIMARY KEY AUTOINCREMENT, usuario TEXT NOT NULL UNIQUE, password TEXT NOT NULL, rol TEXT NOT NULL DEFAULT 'admin');
CREATE TABLE sqlite_sequence(name,seq);
CREATE TABLE estudiantes (id INTEGER PRIMARY KEY AUTOINCREMENT, nombre TEXT NOT NULL, apellido TEXT NOT NULL, cedula TEXT NOT NULL UNIQUE, trayecto TEXT NOT NULL, tramo TEXT NOT NULL, seccion TEXT NOT NULL, calificacion REAL NULL, creado_en TEXT DEFAULT (datetime('now')));
```

Dos detalles de esa salida que no coinciden con el `CREATE TABLE` del [README](../README.md). El primero: `.schema` imprime **una sentencia por línea**, sin partirla, aunque sea larga. El segundo: **no aparece `IF NOT EXISTS`**. La columna no se perdió; la omisión es de la herramienta, que la quita al mostrar el esquema guardado. Y el orden no es el del código: `sqlite_sequence` sale **entre** las dos tablas, porque la crea el motor en el momento de crear la primera tabla con `AUTOINCREMENT`, que en este arranque es `usuarios`.

La segunda tabla no la escribió el proyecto: la crea el motor por el `AUTOINCREMENT`.

**Prueba de las restricciones, sin romper nada.** Trabaja sobre una copia, nunca sobre el archivo vivo:

```bash
cp ~/.local/share/CrudQt/crudqt.db /tmp/prueba.db
sqlite3 /tmp/prueba.db "INSERT INTO estudiantes (nombre,apellido,cedula,trayecto,tramo,seccion)
                         VALUES ('Ana','Prueba','99999999','II','1','B');"   # calificacion sin mencionar → NULL
sqlite3 -header -column /tmp/prueba.db "SELECT id, nombre, calificacion, typeof(calificacion) FROM estudiantes;"
# id  nombre     calificacion  typeof(calificacion)
# --  ---------  ------------  --------------------
# 1   Sebastian  18.0          real
# 2   Ana                      null                 ← el NULL de la sección 4

sqlite3 /tmp/prueba.db "INSERT INTO estudiantes (nombre,apellido,cedula,trayecto,tramo,seccion)
                         VALUES ('Dup','X','32117825','I','1','A');"
# Error: stepping, UNIQUE constraint failed: estudiantes.cedula (19)
```

El `(19)` es el código de retorno de `sqlite3`: distinto de cero, así que un script puede detectarlo.

**Por qué conviene cerrar la aplicación antes.** Con el modo de journal `delete` de la [sección 2.1](#21-el-archivo), lector y escritor **no** conviven libres: un lector que mantiene abierta su transacción bloquea al escritor. Comprobado sobre el motor real —un `sqlite3` con `BEGIN` y un `SELECT` abiertos, y otra conexión intentando `BEGIN IMMEDIATE` + `INSERT`—:

```
Error: stepping, database is locked (5)
```

Esa libertad de «varios lectores y un escritor sin molestarse» es una propiedad de **WAL**, y este proyecto no usa WAL. Con el mismo test tras un `PRAGMA journal_mode=WAL` el escritor no se bloquea nunca. En la práctica, una consulta corta con la CLI mientras la app está quieta sí funciona, porque el bloqueo se toma y se suelta dentro de la sentencia; lo que no funciona es dejar la sesión de la CLI en un `BEGIN`, porque entonces el siguiente guardado de la aplicación fallará con *database is locked*. Cerrar la app elimina la variable entera.

Aun sin bloqueo, lo que sí cambia es la **experiencia**: la ventana principal usa un `QSqlTableModel` que **no vuelve a consultar solo**. Si escribes desde la CLI, la tabla seguirá mostrando los datos antiguos hasta que pulses **Refrescar** (`mainwindow.cpp:147-156`) o cierres sesión y vuelvas a entrar. No es un fallo: es el modelo funcionando como debe.

### 8.3 Con el inspector de Qt Creator

Qt Creator trae un explorador de bases de datos en el panel de proyecto, bajo el nodo **Databases**. El flujo es:

1. Abrir el proyecto en Qt Creator y desplegar **Databases** en el árbol.
2. Botón derecho sobre **Databases** → **Add Connection**.
3. En el formulario: driver **QSQLITE**, host y usuario **vacíos** (SQLite no tiene servidor), y en *Database* la ruta completa al archivo `crudqt.db`.
4. **Test connection** y después **Save**.
5. Desplegar la conexión: se ven las tablas, el esquema, y los datos de cada una. El botón de la cinta lateral abre un editor SQL para escribir consultas contra ella.

Una advertencia honesta: el nodo solo funciona si Qt Creator encuentra el **plugin del driver**. Si la lista de drivers aparece vacía o no hay ninguna base de datos, el problema no es la base de datos sino que Qt Creator no está viendo el `qsqlite` de su propia versión de Qt. Es el mismo plugin que el [zip de Windows](forWindowsBuilt.md#contenido-del-zip) tiene que llevar.

---

## 9. `hashPassword()`: SHA-256 sin salt, con honestidad

### 9.1 Qué hace, exactamente

`database.cpp:108-112`, la función completa:

```cpp
QString hashPassword(const QString &password) {
  return QString(QCryptographicHash::hash(password.toUtf8(),
                                          QCryptographicHash::Sha256)
                     .toHex());
}
```

| Pieza | Qué hace y por qué está |
|---|---|
| `toUtf8()` | Convierte el `QString` a bytes **UTF-8** antes de hashear, para que el hash no dependa de la codificación local del sistema. |
| `QCryptographicHash::Sha256` | SHA-256: 256 bits de salida. Es una función de un solo sentido: no existe una operación que deshaga el hash. |
| `toHex()` | Devuelve los 32 bytes como **64 caracteres hexadecimales** en minúsculas. Eso es lo que se guarda en la columna `password`. |

Comprobado contra la base de datos real. La fila del administrador semilla contiene:

```
8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918
```

Y ese valor es exactamente el SHA-256 de la cadena `admin` en UTF-8, confirmado con una herramienta independiente del proyecto:

```bash
printf 'admin' | sha256sum
# 8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918
```

**Una sola función de hasheo en todo el proyecto.** Se usa en cuatro sitios y en ningún otro se calcula un hash a mano:

| Sitio | Línea | Para qué |
|---|---|---|
| Semilla de la base | `database.cpp:90` | Crear el `admin` / `admin` inicial |
| Validación del login | `logindialog.cpp:46` | Comparar la contraseña escrita con la guardada |
| Alta de administrador | `admindialog.cpp:114` | Guardar el hash de la contraseña nueva |
| Edición de administrador | `admindialog.cpp:217` | Guardar el hash cuando se cambia la contraseña |

Que el algoritmo no pueda divergir entre el login, la semilla y la gestión de administradores es la razón de que viva en una función y no repartido en cuatro sitios.

### 9.2 Lo que el hash protege

| Propiedad | Efecto comprobable |
|---|---|
| La contraseña no está en el archivo | Un `SELECT` sobre `usuarios` devuelve 64 caracteres hexadecimales, no la palabra. Eso es **todo** lo que el hash oculta. |
| El usuario **sí** está | La columna `usuario` se guarda en claro. Nunca fue un secreto y el hash no la toca. |
| Es de un solo sentido | No hay forma de obtener la contraseña a partir del hash; solo se puede probar una candidata y ver si coincide. |
| La comparación es por igualdad | `WHERE password = :p` con `hashPassword(p)` es un `WHERE` normal de una columna `TEXT`. No hay decrypt que reversing. |

**El alcance exacto: el hash protege la contraseña, no el usuario.** Quien abra `crudqt.db` con un editor de texto **sí** encuentra la palabra `admin` escrita en claro, y la encuentra tres veces. La primera es el `DEFAULT 'admin'` del propio esquema. Las otras dos están en la fila semilla, y se ven juntas en el mismo volcado de `strings` sobre el archivo real:

```
admin8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918principal
```

Ahí van los tres campos de la fila, uno detrás de otro: el `admin` de la izquierda es el **usuario**, los 64 hexadecimales son el **hash de la contraseña**, y el `principal` final es el **rol**. Conviene notar la coincidencia: el usuario semilla se llama `admin` y su contraseña también, así que el `admin` de la izquierda parece la contraseña sin hashear. Es casualidad del nombre elegido para la semilla, no una fuga — pero en cualquier otro usuario el nombre sería igual de visible, y por eso la conclusión no depende de ese caso.

Hashear la contraseña no convierte en secreto el resto de la fila. Se protege la *secretez de la contraseña* y nada más. Por eso afirmar que el nombre de usuario tampoco se ve en el archivo sería proteger de más, y en una sección de seguridad eso importa tanto como la otra dirección.

### 9.3 Lo que el hash NO protege — sin adornos

Esto es tan importante como lo anterior, porque **SHA-256 sin salt no es una buena práctica de contraseñas** y no debe presentarse como tal.

| Problema | Qué significa en este caso |
|---|---|
| **Sin salt** | El hash depende solo de la contraseña. Dos administradores con la misma contraseña tienen **el mismo hash**, y se reconoce de un vistazo. |
| **Sin factor de coste** | SHA-256 está diseñada para ser **rápida**, no lenta. Una GPU moderna calcula miles de millones de hashes por segundo. |
| **Tablas precalculadas** | Los hashes sin salt de contraseñas comunes están publicados y se descargan. Probar un diccionario contra el archivo es trivial. |
| **No protege los datos** | Quien tenga acceso de lectura al archivo puede abrirlo con `sqlite3` y leer o modificar los estudiantes. El hash protege la *secretez de la contraseña*, no la *confidencialidad de la base*. |
| **No intenta limitar intentos** | No hay bloqueo tras varios fallos ni retardo: el login acepta un intento por pulsación. |

**La decisión es defendible para esta aplicación, y el motivo es concreto.** En una app de escritorio local, el atacante realista es alguien que ya tiene acceso al sistema de archivos del usuario. Si llega a leer `crudqt.db`, no necesita romper el hash: abre el archivo con `sqlite3` y lee o modifica lo que quiera directamente. El hash evita únicamente que la contraseña sea legible de un vistazo, que es exactamente el riesgo que se quiere tapar aquí. El archivo vive además en el home del usuario, con sus permisos.

### 9.4 Qué habría que cambiar en un sistema real

Si el mismo esquema se usara en una app de red, con varios usuarios y datos personales, SHA-256 sin salt no sería aceptable. Harían falta dos cosas, y las dos están en Qt:

**1. Un algoritmo de derivación con salt e iteraciones.** Qt lo trae en `QPasswordDigestor`:

```cpp
// NO está en este proyecto: es el sustituto si hubiera que cambiar el hasheo.
QByteArray hash = QPasswordDigestor::deriveKeyPbkdf2(
    QCryptographicHash::Sha256, password.toUtf8(), saltAleatorio, iteraciones, 32);
```

El *salt* sería un valor aleatorio distinto por usuario, guardado junto al hash; las iteraciones convierten cada hash en un trabajo caro a propósito. Argon2 o bcrypt son la alternativa más moderna, y no vienen en Qt.

> **Un detalle que sorprende:** `QPasswordDigestor` vive en el módulo **Network** de Qt, no en Core. Usarlo exigiría añadir `Network` al `find_package` y al `target_link_libraries` del `CMakeLists.txt`. Y en Qt 6 la función se llama `deriveKeyPbkdf2`: la antigua `deriveKey` ya no existe, fue dividida en `deriveKeyPbkdf1` y `deriveKeyPbkdf2`.

**2. Una migración de esquema.** Añadir el salt no es solo cambiar una función: haría falta una columna nueva para el salt y otra para el número de iteraciones, más un paso de migración como el de la [sección 6](#6-la-migración-con-pragma-table_info), y decidir qué se hace con los hashes antiguos. Esa es la razón por la que la [sección 12 de Explication.md](Explication.md) insiste en la misma advertencia, y el [README](../README.md) la repite.

---

## Checklist de autoevaluación

Antes de dar SQLite por entendido en este proyecto, responde sin mirar el código:

- [ ] ¿Por qué `calificacion` es `REAL NULL` y no `REAL NOT NULL DEFAULT 0`? ¿Qué dato se perdería?
- [ ] Un estudiante con nota `0.0` y otro sin nota: ¿qué devuelve `typeof(calificacion)` en cada caso y qué se ve en pantalla?
- [ ] ¿En qué archivo y en qué línea se decide que "Sin calificar" se pinte? ¿Por qué esa cadena nunca llega a la base de datos?
- [ ] Si mañana se añadiera una columna a la tabla `usuarios` en el `CREATE TABLE`, ¿qué pasaría con una base de datos ya creada? ¿Qué lo arregla?
- [ ] ¿Por qué `migrarColumnaRol()` consulta `PRAGMA table_info` **antes** de ejecutar el `ALTER TABLE`?
- [ ] ¿Por qué el `ALTER TABLE` necesita `DEFAULT 'admin'`? ¿Qué valor reciben los administradores que ya existían?
- [ ] ¿Qué pasaría si la función `migrarColumnaRol()` no tuviera el `PRAGMA` y ejecutara el `ALTER TABLE` en cada arranque?
- [ ] ¿Qué valor tiene `m_id` en la consulta de cédula duplicada durante un alta? ¿Y durante una edición?
- [ ] ¿Por qué el `INSERT` de estudiantes no menciona `id` ni `creado_en`? ¿Qué parte del trabajo hace la base de datos en lugar de la aplicación?
- [ ] ¿Por qué el listado de administradores pide `SELECT id, usuario, rol` y no `SELECT *`?
- [ ] Si un `UPDATE` de administrador termina con `numRowsAffected() == 0` y el código no lo comprueba, ¿qué vería el usuario?
- [ ] ¿Por qué el blindaje del principal está en el `WHERE` del SQL y no solo en los botones deshabilitados de la UI?
- [ ] Escribe el hash de `admin` y compáralo con el valor guardado en la tabla `usuarios`. ¿Coinciden? Explica por qué el login tiene que hashear la entrada y no puede compararla en claro.
- [ ] ¿Por qué SHA-256 sin salt es aceptable aquí y no lo sería en una aplicación de red? Nombra dos razones concretas.
- [ ] La ruta del archivo es `~/.local/share/CrudQt/crudqt.db`. ¿De dónde sale cada parte de ese path, y por qué la carpeta y el archivo tienen distinta capitalización?

---

## Siguiente paso

Con la base de datos entendida, las dos piezas siguientes son el **Model/View** que la muestra ([Explication.md](Explication.md), sección 6) y el **motor de construcción** que empaqueta todo en un ejecutable de Windows ([forWindowsBuilt.md](forWindowsBuilt.md)).
