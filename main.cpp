#include "database.h"
#include "logindialog.h"
#include "mainwindow.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QPalette>
#include <QString>

namespace {

// Paleta Gruvbox dark (material). Mismos valores que style.qss: el QSS resuelve
// borders, paddings y estados, y la QPalette cubre lo que el QSS no alcanza
// (primitives que dibuja Fusion y widgets pintados nativamente).
constexpr char kBg0[] = "#282828"; // fondo base
constexpr char kBg1[] = "#3c3836"; // paneles, campos, filas alternas
constexpr char kBg2[] = "#504945"; // headers, botones, tooltips
constexpr char kBg3[] = "#665c54"; // bordes y handle de scrollbar
constexpr char kFg0[] = "#fbf1c7"; // texto principal
constexpr char kFg1[] = "#ebdbb2"; // texto secundario
constexpr char kGray[] = "#928374"; // deshabilitado y placeholders
constexpr char kAqua[] = "#8ec07c"; // acento de foco y selección
constexpr char kBlue[] = "#83a598"; // enlaces

// Aplica estilo Fusion, la QPalette oscura y la hoja de estilos. Debe correr
// antes de construir cualquier widget para que el login ya salga tematizado.
void aplicarTemaGruvbox(QApplication &app) {
  // Fusion: única base consistente entre Linux, Windows y macOS.
  app.setStyle(QStringLiteral("Fusion"));

  QPalette pal;
  pal.setColor(QPalette::Window, QColor(kBg0));
  pal.setColor(QPalette::WindowText, QColor(kFg0));
  pal.setColor(QPalette::Base, QColor(kBg1));
  pal.setColor(QPalette::AlternateBase, QColor(kBg1));
  pal.setColor(QPalette::Text, QColor(kFg0));
  pal.setColor(QPalette::Button, QColor(kBg2));
  pal.setColor(QPalette::ButtonText, QColor(kFg0));
  // Highlight en aqua con texto bg0: 7.0:1 de contraste para texto seleccionado.
  pal.setColor(QPalette::Highlight, QColor(kAqua));
  pal.setColor(QPalette::HighlightedText, QColor(kBg0));
  pal.setColor(QPalette::ToolTipBase, QColor(kBg2));
  pal.setColor(QPalette::ToolTipText, QColor(kFg0));
  pal.setColor(QPalette::PlaceholderText, QColor(kGray));
  pal.setColor(QPalette::Link, QColor(kBlue));
  pal.setColor(QPalette::Mid, QColor(kBg3));
  pal.setColor(QPalette::Shadow, QColor(kBg0));
  pal.setColor(QPalette::BrightText, QColor(QStringLiteral("#fb4934")));

  // Estados deshabilitados: el texto se atenúa a gray sobre el fondo del
  // propio control (2.4:1, exento de WCAG por ser estado deshabilitado).
  pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(kGray));
  pal.setColor(QPalette::Disabled, QPalette::Text, QColor(kGray));
  pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(kGray));
  pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(kGray));
  app.setPalette(pal);

  // El QSS va embebido en el binario (resources.qrc) para que el tema no
  // dependa del directorio de trabajo desde el que se lance la app.
  QFile f(QStringLiteral(":/styles/style.qss"));
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qCritical("No se pudo abrir :/styles/style.qss: %s",
              qPrintable(f.errorString()));
    return;
  }
  app.setStyleSheet(QString::fromUtf8(f.readAll()));
}

} // namespace

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  // Estilo y paleta ANTES de cualquier diálogo o ventana.
  aplicarTemaGruvbox(a);

  // Abre, migra y siembra la BD (idempotente).
  if (!initDatabase()) {
    return 1;
  }

  // Login primero
  LoginDialog login;
  if (login.exec() != QDialog::Accepted) {
    return 0; // Usuario canceló o cerró el login
  }

  // CRUD después
  MainWindow w;
  w.show();

  return a.exec();
}
