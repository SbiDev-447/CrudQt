#!/usr/bin/env bash
# deploy-linux-cross.sh — Empaqueta CrudQt.exe para Windows (compilacion cruzada).
#
# Uso:        bash deploy-linux-cross.sh
# Requiere:   el cross-build en build-win/ (cmake --build build-win)
# Producen:   dist/CrudQt-win-x86_64.zip
#
# Rutas de la toolchain portable en ~/devtools (ver cmake/toolchains/windows-x86_64.cmake).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT=${QT_WIN:-"$HOME/devtools/6.8.2/mingw_64"}
LLVM=${LLVM_MINGW:-"$HOME/devtools/llvm-mingw"}
STAGE="$ROOT/dist/CrudQt"
ZIP="$ROOT/dist/CrudQt-win-x86_64.zip"
# Herramienta de solo lectura que responde "¿qué DLL importa este binario?".
OBJDUMP=${OBJDUMP:-"$LLVM/bin/llvm-objdump"}

EXE="$ROOT/build-win/CrudQt.exe"
[ -f "$EXE" ] || { echo "ERROR: $EXE no existe. Ejecuta el cross-build antes." >&2; exit 1; }

# Clasifica como "de Windows" una DLL que el sistema (o la UCRT) ya aporta, de
# modo que no hay que empaquetarla. El filtro es deliberadamente permisivo:
# clasificar de más solo hace que se deje de enviar algo que Windows ya trae,
# mientras que clasificar de menos rompe el zip en la máquina de destino.
# Los nombres van sin extensión porque la comparación se hace en minúsculas y
# el nombre importado no siempre la trae.
esDllDeWindows() {
  case "${1,,}" in
    api-ms-* | ext-ms-*) return 0 ;;
    advapi32 | authz | bcrypt | cabinet | cfgmgr32 | combase | comctl32 | \
    comdlg32 | crypt32 | cryptbase | dbghelp | dcomp | dwrite | dwmapi | \
    gdi32 | gdiplus | imm32 | iphlpapi | kernel32 | mpr | msvcp140 | \
    msvcr120 | msvcrt | netapi32 | normaliz | ntdll | ole32 | oleacc | \
    oleaut32 | opengl32 | powrprof | propsys | psapi | rpcrt4 | sechost | \
    setupapi | shcore | shell32 | shlwapi | user32 | userenv | uxtheme | \
    version | winmm | winspool | ws2_32 | wtsapi32) return 0 ;;
    d3d9 | d3d11 | d3d12 | dxgi | mfplat | mfreadwrite | xinput) return 0 ;;
    *) return 1 ;;
  esac
}

# Recorre los imports declarados por un binario PE e imprime en stdout el
# nombre de cada DLL que el paquete deberia contener. Solo normaliza el nombre
# y descarta lo que ya trae Windows: no decide si falta o no.
importsDeBinario() {
  local binario=$1 linea nombre
  "$OBJDUMP" -p "$binario" 2>/dev/null |
    sed -n 's/^[[:space:]]*DLL Name:[[:space:]]*//p' |
    while IFS= read -r linea; do
      # objdump puede imprimir la ruta completa; Windows busca por nombre.
      nombre=${linea##*/}
      if ! esDllDeWindows "${nombre%.dll}"; then
        printf '%s\n' "$nombre"
      fi
    done
}

echo "== Limpiando staging"
rm -rf "$STAGE" "$ZIP"
mkdir -p "$STAGE/plugins/platforms" "$STAGE/plugins/sqldrivers" "$STAGE/plugins/styles" "$STAGE/plugins/imageformats"

echo "== Copiando ejecutable"
cp "$EXE" "$STAGE/"

echo "== Copiando DLLs de Qt"
cp "$QT/bin/Qt6Core.dll" "$QT/bin/Qt6Gui.dll" "$QT/bin/Qt6Sql.dll" \
   "$QT/bin/Qt6Widgets.dll" "$QT/bin/Qt6Svg.dll" "$STAGE/"

echo "== Copiando runtime de LLVM-MinGW (UCRT ya viene con Windows 10+)"
cp "$LLVM/x86_64-w64-mingw32/bin/libc++.dll" \
   "$LLVM/x86_64-w64-mingw32/bin/libunwind.dll" \
   "$LLVM/x86_64-w64-mingw32/bin/libwinpthread-1.dll" \
   "$STAGE/"

# Hay DOS runtimes de C++ en juego y el zip necesita los dos. El ejecutable se
# compila con LLVM-MinGW, que esta construido sobre libc++, asi que importa
# libc++.dll. El Qt de $QT, en cambio, fue compilado con GCC-MinGW y sus DLL
# (y las de todos sus plugins) importan libstdc++-6.dll y libgcc_s_seh-1.dll.
# Los nombres no chocan, asi que conviven sin problema: enviar solo uno deja
# el ejecutable sin poder cargar Qt. Notese que estas dos salen de $QT/bin y
# NO de $LLVM: llvm-mingw no distribuye libstdc++.
echo "== Copiando runtime de GCC-MinGW (lo necesitan Qt y sus plugins)"
cp "$QT/bin/libstdc++-6.dll" \
   "$QT/bin/libgcc_s_seh-1.dll" \
   "$STAGE/"

echo "== Copiando plugins de Qt"
cp "$QT/plugins/platforms/qwindows.dll"    "$STAGE/plugins/platforms/"
cp "$QT/plugins/sqldrivers/qsqlite.dll"    "$STAGE/plugins/sqldrivers/"
cp "$QT/plugins/styles/qmodernwindowsstyle.dll" "$STAGE/plugins/styles/"
cp "$QT/plugins/imageformats/qgif.dll"     "$STAGE/plugins/imageformats/"
cp "$QT/plugins/imageformats/qico.dll"     "$STAGE/plugins/imageformats/"
cp "$QT/plugins/imageformats/qjpeg.dll"    "$STAGE/plugins/imageformats/"
cp "$QT/plugins/imageformats/qsvg.dll"     "$STAGE/plugins/imageformats/"

# Verificacion transitiva real. La anterior solo miraba que estuvieran las DLL
# que el propio script copia, asi que un zip con las 7 correctas y tres
# dependencias ausentes pasaba el filtro y el fallo aparebia en Windows. Ahora
# se pregunta a cada binario del staging que importa de verdad y se contrasta
# con lo que realmente se empaqueta. Va ANTES de comprimir: si algo falta, es
# preferible no dejar un zip roto en disco.
echo "== Verificando dependencias de todos los binarios del staging"
if ! command -v "$OBJDUMP" >/dev/null 2>&1; then
  echo "ERROR: no se encuentra llvm-objdump (prueba OBJDUMP=/ruta/a/llvm-objdump)." >&2
  exit 1
fi

declare -A EMBARCADAS=()
declare -A FALTAN=()
escaneados=0

# Primero el conjunto completo de lo que se empaqueta, por dos motivos: el
# orden de `find` no garantiza que Qt6Core.dll se haya visto ya cuando se
# revisen los imports de CrudQt.exe, y un binario solo puede considerarse
# resuelto por su nombre, no por su posición en el recorrido.
while IFS= read -r -d '' binario; do
  EMBARCADAS[${binario##*/}]=1
  escaneados=$((escaneados + 1))
done < <(find "$STAGE" -type f \( -name '*.exe' -o -name '*.dll' \) -print0)

# Despues se pregunta a cada binario que importa. Solo importa el nombre: la
# DLL puede estar en la raiz o en plugins/<categoria>/ y Windows la busca en
# el directorio de la aplicación.
while IFS= read -r -d '' binario; do
  while IFS= read -r dependencia; do
    [ -n "$dependencia" ] || continue
    [ -n "${EMBARCADAS[$dependencia]+si}" ] || FALTAN[$dependencia]=1
  done < <(importsDeBinario "$binario")
done < <(find "$STAGE" -type f \( -name '*.exe' -o -name '*.dll' \) -print0)

# Se recogen TODAS las ausencias antes de reportar: abortar en la primera
# obligaria a repetir el script N veces para descubrir N dependencias.
if [ "${#FALTAN[@]}" -gt 0 ]; then
  {
    echo "ERROR: hay dependencias importadas que no estan en el zip:" >&2
    printf '   - %s\n' "${!FALTAN[@]}" | sort >&2
    echo "   Copia la DLL a dist/CrudQt/ y anadela al script; no se empaqueta a proposito." >&2
  }
  exit 1
fi

echo "   OK $escaneados binarios escaneados, ${#EMBARCADAS[@]} DLLs empaquetadas, 0 faltantes"
echo "   (llvm-objdump: $OBJDUMP)"

echo "== Empaquetando"
(cd "$ROOT/dist" && zip -qr "$(basename "$ZIP")" "$(basename "$STAGE")")

echo ""
echo "Listo: $ZIP"
du -h "$ZIP"
echo ""
echo "Para el amigo: descomprimir el zip y ejecutar CrudQt.exe (no instala nada)."
