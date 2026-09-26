# Compilar CrudQt para Windows desde Linux

Guía completa para obtener **`dist/CrudQt-win-x86_64.zip`** (13 MB) desde un equipo Linux, sin Windows, sin VirtualBox y sin instalar nada en el sistema. Quien recibe el zip lo descomprime y ejecuta `CrudQt.exe`: no instala nada, no necesita Qt y no necesita un instalador.

Todo el toolchain (Qt, compilador, `aqtinstall`) vive en `~/devtools`, una carpeta portátil. El sistema no se modifica: no hay paquetes del sistema, ni `~/.local`, ni permisos de root.

## Camino rápido

| Paso | Qué haces | Resultado |
|------|-----------|-----------|
| [1](#paso-1--toolchain-portable-en-devtools) | Descargar Qt (Windows + Linux) y LLVM-MinGW en `~/devtools` | Compilador y Qt listos |
| [2](#paso-2--configurar) | Configurar con el toolchain file y `QT_HOST_PATH` | `build-win/` generado |
| [3](#paso-3--compilar) | `cmake --build build-win -j$(nproc)` | `build-win/CrudQt.exe` |
| [4](#paso-4--empaquetar) | `bash deploy-linux-cross.sh` | `dist/CrudQt-win-x86_64.zip` |
| [5](#paso-5--verificar) | `file`, `llvm-objdump`, `unzip -l` | Binario PE y zip completo |

## Introducción: por qué no basta con el Qt de Linux

Qt no es una biblioteca de código fuente que se pueda recompilar a voluntad. Lo que se descarga son **binarios ya compilados para una plataforma concreta**, y cada plataforma tiene su propio formato de ejecutable y de biblioteca:

| | Linux | Windows |
|---|---|---|
| Formato del binario | ELF | PE (Portable Executable) |
| Bibliotecas | `.so` (compartidas) | `.dll` |
| Qué produce el compilador | `clang++` de Linux | `x86_64-w64-mingw32-clang++` de LLVM-MinGW |

Por lo tanto, si en Linux enlazas contra el Qt instalado en `/usr`, obtienes un binario ELF con dependencias `.so`: Windows no lo puede ejecutar. Para `CrudQt.exe` hacen falta **dos cosas a la vez**:

1. **Un compilador que genere PE**, no ELF. Ese es el papel de LLVM-MinGW: un clang con triplet `x86_64-w64-mingw32` y sus propias cabeceras y bibliotecas de Windows.
2. **Un Qt compilado para Windows**, no el de Linux. Un Qt para Linux no contiene `Qt6Core.dll`, y aunque lo tuviera, sus bibliotecas también serían `.dll` de otro compilador.

Y hay un tercer requisito, menos evidente: **las herramientas de Qt (moc, uic, rcc) son programas nativos**. `moc` de la instalación de Windows es `moc.exe`, un binario PE que Linux no puede ejecutar. Por eso el paso 1 descarga **dos** instalaciones de Qt: la de Windows (bibliotecas del binario final) y la de Linux (herramientas que se ejecutan durante la compilación).

**Qué se obtiene al terminar:** un zip de 13 MB con el ejecutable, las DLL de Qt, el runtime del compilador y los plugins. El usuario final descomprime y ejecuta; funciona en cualquier Windows x64 con Windows 10 o posterior.

---

## Paso 1 — Toolchain portable en `~/devtools`

Tres piezas, todas en `~/devtools`. Ninguna toca el sistema.

### 1.1 `aqtinstall` para descargar Qt

Qt Installer no funciona sin interfaz gráfica en un servidor, así que se usa `aqtinstall`. En Debian y Ubuntu, PEP 668 bloquea `pip install --user` con el error `externally-managed-environment`; por eso se instala con `--target` en una carpeta propia:

```bash
pip install --target ~/devtools/aqt-pylib aqtinstall
```

**Invócalo siempre con `PYTHONPATH`**, para que Python encuentre la instalación aislada:

```bash
PYTHONPATH=~/devtools/aqt-pylib python3 -m aqt
```

### 1.2 Qt para Windows (target)

Son las bibliotecas y headers contra los que se compila `CrudQt.exe`:

```bash
PYTHONPATH=~/devtools/aqt-pylib python3 -m aqt install-qt windows desktop 6.8.2 win64_mingw -O ~/devtools
```

Se instala en **`~/devtools/6.8.2/mingw_64`**. Aquí están `bin/Qt6Core.dll`, `lib/*.a`, `include/` y el `moc.exe` que no se puede ejecutar en Linux.

### 1.3 Qt para Linux (host, solo herramientas)

Las herramientas AUTOMOC/AUTOUIC/AUTORCC deben ejecutarse **en la máquina de compilación**, así que hacen falta sus versiones nativas:

```bash
PYTHONPATH=~/devtools/aqt-pylib python3 -m aqt install-qt linux desktop 6.8.2 linux_gcc_64 -O ~/devtools
```

Se instala en **`~/devtools/6.8.2/gcc_64`** y aporta `libexec/moc` (un binario ELF que sí corre en Linux).

> **El Qt de los paquetes de Debian (`/usr`) no sirve como host.** La instalación de `aqtinstall` y la de la distribución tienen un layout distinto: las herramientas no quedan donde `find_package` las espera, y el resultado es un fallo de AUTOMOC aunque el path apunte a una instalación aparentemente correcta.

### 1.4 LLVM-MinGW (compilador clang para Windows)

Descárgalo desde la **última release** del repositorio [llvm-mingw/llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) (por ejemplo `20260922`, con clang 23.1.2) y elige la variante **`ucrt`**, no la `msvcrt`:

```
llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64.tar.xz
```

> **Por qué `ucrt` y no `msvcrt`:** los builds oficiales de Qt 6 que usa `aqtinstall` están construidos contra la UCRT (biblioteca universal de C en Windows 10+). Enlazar con un runtime `msvcrt` produce fallos de símbolos al compilar contra ese Qt.

Descomprime el archivo en `~/devtools`, de modo que el compilador quede en:

```
~/devtools/llvm-mingw/bin/x86_64-w64-mingw32-clang
```

Las DLL de runtime que luego se empaquetan están en:

```
~/devtools/llvm-mingw/x86_64-w64-mingw32/bin/    → libc++.dll, libunwind.dll, libwinpthread-1.dll
```

### Inventario final

| Ruta en `~/devtools` | Papel |
|---|---|
| `aqt-pylib/` | `aqtinstall` 3.3.0 aislado (se invoca con `PYTHONPATH`) |
| `6.8.2/mingw_64/` | Qt 6.8.2 para Windows: headers, bibliotecas y `.dll` del ejecutable final |
| `6.8.2/gcc_64/` | Qt 6.8.2 para Linux: `moc`, `uic`, `rcc` ejecutables durante la compilación |
| `llvm-mingw/` | Compilador clang con triplet `x86_64-w64-mingw32` (variante `ucrt`) |
| `llvm-mingw/x86_64-w64-mingw32/bin/` | `libc++.dll`, `libunwind.dll`, `libwinpthread-1.dll` para el zip |

---

## Paso 2 — Configurar

El toolchain file del repositorio (`cmake/toolchains/windows-x86_64.cmake`) es:

```cmake
set(DEVTOOLS "$ENV{HOME}/devtools")

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER "${DEVTOOLS}/llvm-mingw/bin/x86_64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${DEVTOOLS}/llvm-mingw/bin/x86_64-w64-mingw32-clang++")

# El Qt for Windows (target) es la unica fuente de librerias/headers.
set(CMAKE_FIND_ROOT_PATH "${DEVTOOLS}/6.8.2/mingw_64")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

Y el comando de configuración:

```bash
cmake -S . -B build-win \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-x86_64.cmake \
  -DQT_HOST_PATH=$HOME/devtools/6.8.2/gcc_64 \
  -DQT_HOST_PATH_CMAKE_DIR=$HOME/devtools/6.8.2/gcc_64/lib/cmake \
  -DCMAKE_BUILD_TYPE=Release
```

**Qué hace cada cosa:**

| Variable | Para qué sirve |
|---|---|
| `CMAKE_TOOLCHAIN_FILE` | Cambia el sistema destino a Windows y fija el compilador clang de LLVM-MinGW. |
| `QT_HOST_PATH` | Le dice a Qt dónde están las **herramientas nativas** (el `moc` de Linux). Sin esto, AUTOMOC intenta ejecutar `moc.exe`. |
| `QT_HOST_PATH_CMAKE_DIR` | Ruta explícita al árbol `lib/cmake` del Qt de host, para que el escaneo de módulos no dependa de la instalación. |
| `CMAKE_BUILD_TYPE=Release` | Binario optimizado. |

**Por qué importa `CMAKE_FIND_ROOT_PATH_MODE_* = ONLY`:** obliga a que la búsqueda de cabeceras y bibliotecas ocurra **solo** dentro de `~/devtools/6.8.2/mingw_64`. Sin esas cuatro líneas, el compilador puede terminar incluyendo cabeceras de `/usr/include` —headers de Linux— mezcladas con las de Windows. Eso no falla de forma visible: produce errores extraños más adelante (tipos incompatibles, `std::` duplicado) que son muy difíciles de diagnosticar. `MODE_PROGRAM NEVER` es la excepción: los ejecutables (el propio compilador) sí se buscan en el `PATH` del sistema.

---

## Paso 3 — Compilar

```bash
cmake --build build-win -j$(nproc)
```

`CMAKE_AUTOMOC`, `CMAKE_AUTOUIC` y `CMAKE_AUTORCC` están activos en el `CMakeLists.txt` y se ejecutan con las herramientas del Qt de Linux. El resultado es:

```
build-win/CrudQt.exe
```

El `CMakeLists.txt` añade además `cmake/win_argc_stub.cpp` **solo** en compilación cruzada:

```cmake
if(CMAKE_CXX_PLATFORM_ID STREQUAL "MinGW" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list(APPEND PROJECT_SOURCES cmake/win_argc_stub.cpp)
endif()
```

El motivo está en [Resolución de problemas](#resolución-de-problemas).

---

## Paso 4 — Empaquetar

```bash
bash deploy-linux-cross.sh
```

El script staging en `dist/CrudQt/` y genera **`dist/CrudQt-win-x86_64.zip`** (13 MB). Antes de comprimir, verifica que las siete DLL principales estén presentes y aborta si falta alguna.

### Contenido del zip

| Archivo | Origen | Para qué |
|---|---|---|
| `CrudQt.exe` | `build-win/` | El ejecutable (subsistema gráfico, sin consola). |
| `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Sql.dll`, `Qt6Widgets.dll` | `~/devtools/6.8.2/mingw_64/bin/` | Bibliotecas de Qt que importa el ejecutable. |
| `libc++.dll`, `libunwind.dll`, `libwinpthread-1.dll` | `~/devtools/llvm-mingw/x86_64-w64-mingw32/bin/` | Runtime de C++, desenrollado de excepciones y hilos. |
| `plugins/platforms/qwindows.dll` | Qt (Windows) | **Imprescindible**: sin él la aplicación no arranca en Windows. |
| `plugins/sqldrivers/qsqlite.dll` | Qt (Windows) | Driver de SQLite; sin él no hay base de datos. |
| `plugins/styles/qmodernwindowsstyle.dll` | Qt (Windows) | Estilo nativo de Windows. |
| `plugins/imageformats/qgif.dll`, `qico.dll`, `qjpeg.dll`, `qsvg.dll` | Qt (Windows) | Formatos de imagen admitidos por Qt. |

> **Las DLL `api-ms-win-crt-*` (UCRT) no se copian a propósito.** `CrudQt.exe` las importa (lo confirma `llvm-objdump`), pero Windows 10 y posteriores ya las incluyen en el sistema. Copiarlas al zip solo añadiría peso y riesgo de versiones contradictorias.

---

## Paso 5 — Verificar

```bash
file build-win/CrudQt.exe
# build-win/CrudQt.exe: PE32+ executable for MS Windows 6.00 (GUI), x86-64, 18 sections

~/devtools/llvm-mingw/bin/llvm-objdump -p build-win/CrudQt.exe | grep "DLL Name"
#   DLL Name: Qt6Core.dll
#   DLL Name: Qt6Gui.dll
#   DLL Name: Qt6Sql.dll
#   DLL Name: Qt6Widgets.dll
#   DLL Name: KERNEL32.dll
#   DLL Name: libc++.dll
#   DLL Name: libunwind.dll
#   DLL Name: api-ms-win-crt-stdio-l1-1-0.dll
#   ...            (el resto son DLL de la UCRT y de Windows: SHELL32, etc.)
#   DLL Name: SHELL32.dll

unzip -l dist/CrudQt-win-x86_64.zip
```

Qué debes comprobar en cada salida:

| Comprobación | Resultado esperado |
|---|---|
| `file` | **PE32+ ... (GUI), x86-64**. Si dice ELF, se compiló con el Qt de Linux. |
| `DLL Name` | Las cuatro `Qt6*.dll` y `libc++.dll` + `libunwind.dll`. |
| `unzip -l` | Las 21 entradas: ejecutable, 7 DLL, 4 subcarpetas de plugins y sus 7 plugins. |

> **Advertencia honesta:** el `.exe` **no se puede ejecutar en Linux**, así que estos comandos verifican la estructura del binario, no su comportamiento. La prueba real (arranque, login, base de datos) tiene que hacerse en Windows. Lo que sí queda verificado aquí es que el formato, las importaciones y el paquete están completos.

---

## Resolución de problemas

### 1. `undefined symbol: __imp___argc`

El error más difícil de esta compilación cruzada. **Causa real:** `libQt6EntryPoint.a`, incluido en los builds oficiales de Qt para MinGW, espera los símbolos *dllimport* del CRT `__imp___argc`, `__imp___argv` y `__imp___wargv`. El CRT de LLVM-MinGW no los expone así: accede a la línea de comandos por otra vía (`__p___argc`), de modo que el enlazador no encuentra lo que la biblioteca de Qt pide.

**Solución ya integrada en el repositorio:** `cmake/win_argc_stub.cpp` define esos tres símbolos con ensamblador en línea, apuntando a datos locales:

```cpp
extern "C" {
int _crudqt_stub_argc = 0;
void *_crudqt_stub_argv = 0;
void *_crudqt_stub_wargv = 0;
}

__asm__(".globl __imp___argc\n.set __imp___argc, _crudqt_stub_argc\n" ...);
```

Es seguro porque `main()` de esta aplicación no usa `argc` ni `argv`: el `EntryPoint` de Qt que los invoca los recibe con valores nulos y la aplicación arranca igual.

> **El archivo debe ser `.cpp` con `extern "C"`, no `.c`.** El proyecto declara `project(CrudQt VERSION 0.1 LANGUAGES CXX)`: CMake descarta **en silencio** las fuentes de idiomas no habilitados, así que un `win_argc_stub.c` se ignoraría sin ningún aviso y el error de enlazado volvería sin explicación.

### 2. Errores raros después de reconfigurar el mismo `build-win`

**Causa real:** `CMakeCache.txt` conserva el compilador, el sistema y las variables de la configuración anterior. Si configuraste una vez sin el toolchain file (o con otro), la caché queda envenenada y CMake mezcla resultados incompatibles: includes de Linux con bibliotecas de Windows, o un compilador que ya no es el del caché.

**Solución:** borrar y reconfigurar desde cero.

```bash
rm -rf build-win
# y repetir el Paso 2
```

Regla práctica: **una configuración = una carpeta de build**. Cambia el toolchain, cambia de carpeta.

### 3. Contaminación con `/usr/include`

**Causa real:** faltan (o están mal escritas) las variables `CMAKE_FIND_ROOT_PATH_MODE_INCLUDE`/`LIBRARY`/`PACKAGE` con valor `ONLY`. Entonces `find_package` y el include path aceptan rutas del sistema y el compilador mezcla headers ELF con bibliotecas PE.

**Solución:** comprobar que el toolchain file contiene las cuatro líneas `CMAKE_FIND_ROOT_PATH_MODE_*` y que `CMAKE_FIND_ROOT_PATH` apunta a `~/devtools/6.8.2/mingw_64`.

### 4. AUTOMOC falla con `moc.exe`

**Causa real:** el Qt de Windows aporta `moc.exe`, un binario PE que Linux no puede ejecutar. AUTOMOC necesita un ejecutable nativo, y por eso existe el Qt de host.

**Solución:** pasar `-DQT_HOST_PATH=$HOME/devtools/6.8.2/gcc_64` y `-DQT_HOST_PATH_CMAKE_DIR=$HOME/devtools/6.8.2/gcc_64/lib/cmake` en el paso 2.

### 5. El zip arranca pero la ventana no aparece

**Causa real:** falta `plugins/platforms/qwindows.dll`. Qt lo carga en tiempo de ejecución, así que el error aparece en la máquina de destino y no en la compilación.

**Solución:** confirma que `qwindows.dll` está en `plugins/platforms/` dentro del zip. El script de empaquetado lo copia siempre; si falta, el zip está incompleto.

### 6. Ventana de SmartScreen al abrir el ejecutable

`CrudQt.exe` no está firmado. Windows lo marca como procedente de un editor desconocido y bloquea la ejecución por defecto. El usuario debe hacer clic en **Más información → Ejecutar de todas formas**. Es el comportamiento normal de cualquier binario portable sin firma de código.

---

## Checklist final

### Toolchain (`~/devtools`)

- [ ] `~/devtools/llvm-mingw/bin/x86_64-w64-mingw32-clang++` existe (variante **ucrt**).
- [ ] `~/devtools/6.8.2/mingw_64/bin/Qt6Core.dll` existe (Qt para Windows).
- [ ] `~/devtools/6.8.2/gcc_64/libexec/moc` existe y **no** es un `.exe` (Qt de host).
- [ ] `~/devtools/llvm-mingw/x86_64-w64-mingw32/bin/` contiene `libc++.dll`, `libunwind.dll` y `libwinpthread-1.dll`.

### Compilación

- [ ] `rm -rf build-win` antes de reconfigurar si se cambió el toolchain.
- [ ] La configuración incluye `CMAKE_TOOLCHAIN_FILE`, `QT_HOST_PATH` y `QT_HOST_PATH_CMAKE_DIR`.
- [ ] `cmake --build build-win -j$(nproc)` termina sin errores de enlazado.
- [ ] `file build-win/CrudQt.exe` reporta **PE32+ ... x86-64**.

### Paquete

- [ ] `bash deploy-linux-cross.sh` terminó sin `FALTA ...dll`.
- [ ] `dist/CrudQt-win-x86_64.zip` existe y ronda los 13 MB.
- [ ] `unzip -l` muestra 21 entradas con `plugins/platforms/qwindows.dll` y `plugins/sqldrivers/qsqlite.dll`.

### En Windows (prueba real, no automatizable desde Linux)

- [ ] Descomprimir el zip en una carpeta cualquiera y ejecutar `CrudQt.exe` sin instalar nada.
- [ ] La ventana de login aparece con el tema Gruvbox.
- [ ] `admin / admin` accede y la tabla de estudiantes carga desde SQLite.
- [ ] Agregar y editar un estudiante funciona.

---

## Siguiente paso

Prueba el flujo completo en una máquina Windows y, si todo funciona, distribuye `dist/CrudQt-win-x86_64.zip`. Para compilar nativamente en Windows existe además `deploy-windows.bat`, que usa `windeployqt` y no requiere compilación cruzada.
