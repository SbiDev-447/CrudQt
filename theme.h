#ifndef THEME_H
#define THEME_H

#include <QColor>

// Tema de la aplicación: dos variantes de Gruvbox (dark y light) con la misma
// estructura de QSS. El tema no vive en ningún widget: se persiste en QSettings
// ("CrudQt", "CrudQt", clave "tema"), main lo aplica al arrancar y MainWindow lo
// cambia desde el menú de ajustes.

// Aplica estilo Fusion, la QPalette del tema y su hoja de estilos desde el
// recurso. Es idempotente: se puede llamar en cualquier momento para repintar
// toda la app (main la llama antes de construir nada, el menú la llama al
// cambiar el tema).
void aplicarTema(bool oscuro);

// Lee el tema persistido. Sin valor guardado se asume el tema oscuro.
bool temaOscuroGuardado();

// Guarda el tema elegido. NO lo aplica: guardar (persistencia) y pintar
// (presentación) son responsabilidades distintas, y quien cambia el tema en
// runtime llama a aplicarTema() justo después.
void guardarTemaOscuro(bool oscuro);

// Color de texto secundario (fg2) del tema aplicado. Lo consultan las vistas
// para atenuar placeholders como "Sin calificar" sin fijar un color por tema:
// el gray oficial (#928374) no llega a 4.5:1 sobre fondos claros.
QColor colorTextoSecundario();

#endif // THEME_H
