#include "database.h"
#include "logindialog.h"
#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QDialog>

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  // Tema y paleta ANTES de cualquier diálogo o ventana: el login ya sale con el
  // tema elegido. Se lee de QSettings una sola vez; si el usuario cambia de
  // tema desde el menú de ajustes, MainWindow lo vuelve a aplicar.
  aplicarTema(temaOscuroGuardado());

  // Abre, migra y siembra la BD (idempotente).
  if (!initDatabase()) {
    return 1;
  }

  // Bucle login -> app: se repite SOLO si el usuario pidió cerrar sesión. El
  // flag se pone a true únicamente desde la señal cerrarSesion(); cerrar la
  // ventana con la X no la emite, y entonces el bucle termina y la app sale.
  bool repetirLogin = false;
  while (true) {
    LoginDialog login;
    if (login.exec() != QDialog::Accepted) {
      break; // El usuario canceló o cerró el login: no hay sesión que abrir.
    }

    MainWindow w;
    // Conectado antes de show() para no perder un cierre de sesión inmediato.
    QObject::connect(&w, &MainWindow::cerrarSesion, &w,
                     [&repetirLogin, &w] {
                       repetirLogin = true;
                       w.close();
                     });
    w.show();

    // exec() retorna al cerrarse la última ventana (quitOnLastWindowClosed).
    a.exec();

    if (!repetirLogin) {
      break; // Se cerró con la X: termina la app.
    }
    repetirLogin = false;
  }

  return 0;
}
