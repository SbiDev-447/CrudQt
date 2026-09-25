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

EXE="$ROOT/build-win/CrudQt.exe"
[ -f "$EXE" ] || { echo "ERROR: $EXE no existe. Ejecuta el cross-build antes." >&2; exit 1; }

echo "== Limpiando staging"
rm -rf "$STAGE" "$ZIP"
mkdir -p "$STAGE/plugins/platforms" "$STAGE/plugins/sqldrivers" "$STAGE/plugins/styles" "$STAGE/plugins/imageformats"

echo "== Copiando ejecutable"
cp "$EXE" "$STAGE/"

echo "== Copiando DLLs de Qt"
cp "$QT/bin/Qt6Core.dll" "$QT/bin/Qt6Gui.dll" "$QT/bin/Qt6Sql.dll" "$QT/bin/Qt6Widgets.dll" "$STAGE/"

echo "== Copiando runtime de LLVM-MinGW (UCRT ya viene con Windows 10+)"
cp "$LLVM/x86_64-w64-mingw32/bin/libc++.dll" \
   "$LLVM/x86_64-w64-mingw32/bin/libunwind.dll" \
   "$LLVM/x86_64-w64-mingw32/bin/libwinpthread-1.dll" \
   "$STAGE/"

echo "== Copiando plugins de Qt"
cp "$QT/plugins/platforms/qwindows.dll"    "$STAGE/plugins/platforms/"
cp "$QT/plugins/sqldrivers/qsqlite.dll"    "$STAGE/plugins/sqldrivers/"
cp "$QT/plugins/styles/qmodernwindowsstyle.dll" "$STAGE/plugins/styles/"
cp "$QT/plugins/imageformats/qgif.dll"     "$STAGE/plugins/imageformats/"
cp "$QT/plugins/imageformats/qico.dll"     "$STAGE/plugins/imageformats/"
cp "$QT/plugins/imageformats/qjpeg.dll"    "$STAGE/plugins/imageformats/"
cp "$QT/plugins/imageformats/qsvg.dll"     "$STAGE/plugins/imageformats/"

echo "== Empaquetando"
(cd "$ROOT/dist" && zip -qr "$(basename "$ZIP")" "$(basename "$STAGE")")

echo "== Verificando import de DLLs en el staging"
for dll in Qt6Core Qt6Gui Qt6Sql Qt6Widgets libc++ libunwind libwinpthread-1; do
    if [ -f "$STAGE/$dll.dll" ]; then
        echo "   OK $dll.dll"
    else
        echo "   FALTA $dll.dll" >&2
        exit 1
    fi
done

echo ""
echo "Listo: $ZIP"
du -h "$ZIP"
echo ""
echo "Para el amigo: descomprimir el zip y ejecutar CrudQt.exe (no instala nada)."