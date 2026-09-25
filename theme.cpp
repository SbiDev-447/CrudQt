#include "theme.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QSettings>
#include <QString>

namespace {

// Colores de cada tema. Se replican en los QSS: el QSS resuelve bordes,
// paddings y estados, y la QPalette cubre lo que el QSS no alcanza (primitives
// que dibuja Fusion y widgets pintados nativamente).
struct Tema {
  bool oscuro;
  const char *bg0;  // fondo base (Window)
  const char *bg1;  // paneles, campos, filas alternas (Base)
  const char *bg2;  // headers, botones, tooltips (Button)
  const char *bg3;  // bordes y handle de scrollbar
  const char *fg0;  // texto principal
  const char *fg1;  // texto secundario
  const char *fg2;  // texto atenuado legible (títulos, "Sin calificar")
  const char *gray; // deshabilitado y placeholders
  const char *red;
  const char *green;
  const char *yellow; // primarios
  const char *blue;
  const char *purple;
  const char *aqua; // acento de foco y selección
  const char *orange;
};

// Gruvbox dark (material).
constexpr Tema kDark = {
    /*oscuro=*/true,  "#282828", "#3c3836", "#504945", "#665c54",
    /*fg0*/           "#fbf1c7", "#ebdbb2", "#d5c4a1", "#928374",
    /*acentos*/       "#fb4934", "#b8bb26", "#fabd2f", "#83a598",
                     "#d3869b", "#8ec07c", "#fe8019"};

// Gruvbox light (oficial): mismos roles con los valores claros de la paleta.
// El único campo que no es una inversión literal es fg2, que en light lleva el
// gris de texto #504945 en vez del gray oficial #928374: sobre fondos claros el
// gray solo da 3.2:1 y el texto atenuado aún habilitado (pestañas, barra de
// estado) necesita >= 4.5:1.
constexpr Tema kLight = {
    /*oscuro=*/false, "#fbf1c7", "#ebdbb2", "#d5c4a1", "#bdae93",
    /*fg0*/           "#282828", "#3c3836", "#504945", "#928374",
    /*acentos*/       "#cc241d", "#79740e", "#d79921", "#458588",
                     "#b16286", "#689d6a", "#d65d0e"};

constexpr auto kOrganizacion = "CrudQt";
constexpr auto kAplicacion = "CrudQt";
constexpr auto kClaveTema = "tema";
constexpr auto kValorDark = "dark";
constexpr auto kValorLight = "light";

QSettings ajustes() {
  return QSettings(QString::fromLatin1(kOrganizacion),
                   QString::fromLatin1(kAplicacion));
}

const Tema &temaDe(bool oscuro) { return oscuro ? kDark : kLight; }

// Tema aplicado, lo fija aplicarTema(). Evita pasar el flag por toda la app
// para consultas puntuales de color (ver colorTextoSecundario).
bool gOscuro = true;

} // namespace

void aplicarTema(bool oscuro) {
  QApplication &app = *qApp;
  const Tema &t = temaDe(oscuro);

  // Fusion: única base consistente entre Linux, Windows y macOS.
  app.setStyle(QStringLiteral("Fusion"));

  QPalette pal;
  pal.setColor(QPalette::Window, QColor(t.bg0));
  pal.setColor(QPalette::WindowText, QColor(t.fg0));
  pal.setColor(QPalette::Base, QColor(t.bg1));
  pal.setColor(QPalette::AlternateBase, QColor(t.bg1));
  pal.setColor(QPalette::Text, QColor(t.fg0));
  pal.setColor(QPalette::Button, QColor(t.bg2));
  pal.setColor(QPalette::ButtonText, QColor(t.fg0));
  // Selección en aqua con el texto de más contraste del tema: dark usa bg0
  // (7.0:1) y light fg0 (4.7:1), el único acento claro que llega a AA con
  // texto oscuro. blue en light se quedaría en 3.7:1.
  pal.setColor(QPalette::Highlight, QColor(t.aqua));
  pal.setColor(QPalette::HighlightedText, QColor(oscuro ? t.bg0 : t.fg0));
  pal.setColor(QPalette::ToolTipBase, QColor(t.bg2));
  pal.setColor(QPalette::ToolTipText, QColor(t.fg0));
  pal.setColor(QPalette::PlaceholderText, QColor(t.gray));
  pal.setColor(QPalette::Link, QColor(t.blue));
  pal.setColor(QPalette::Mid, QColor(t.bg3));
  pal.setColor(QPalette::Shadow, QColor(t.bg0));
  pal.setColor(QPalette::BrightText, QColor(t.red));

  // Estados deshabilitados: el texto se atenúa a gray sobre el fondo del
  // propio control (exento de WCAG por ser estado deshabilitado).
  pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(t.gray));
  pal.setColor(QPalette::Disabled, QPalette::Text, QColor(t.gray));
  pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(t.gray));
  pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(t.gray));
  app.setPalette(pal);

  // El QSS va embebido en el binario (resources.qrc) para que el tema no
  // dependa del directorio de trabajo desde el que se lance la app.
  const QString recurso = QStringLiteral(":/styles/style-%1.qss")
                              .arg(oscuro ? kValorDark : kValorLight);
  QFile f(recurso);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qCritical("No se pudo abrir %s: %s", qPrintable(recurso),
              qPrintable(f.errorString()));
    return;
  }
  app.setStyleSheet(QString::fromUtf8(f.readAll()));

  gOscuro = oscuro;
}

bool temaOscuroGuardado() {
  return ajustes().value(QString::fromLatin1(kClaveTema),
                         QString::fromLatin1(kValorDark)) !=
         QString::fromLatin1(kValorLight);
}

void guardarTemaOscuro(bool oscuro) {
  ajustes().setValue(QString::fromLatin1(kClaveTema),
                     QString::fromLatin1(oscuro ? kValorDark : kValorLight));
}

QColor colorTextoSecundario() { return QColor(temaDe(gOscuro).fg2); }
