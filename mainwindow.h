#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlTableModel>
#include <QVariant>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// Modelo de estudiantes: muestra calificaciones NULL como "Sin calificar".
class StudentTableModel : public QSqlTableModel {
  Q_OBJECT

public:
  using QSqlTableModel::QSqlTableModel;
  // Una columna virtual más para acciones (no existe en la BD): columnCount()
  // devuelve las columnas de la tabla más una al final.
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
};

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

signals:
  // El usuario pidió cerrar sesión desde el menú de ajustes: main() vuelve a
  // mostrar el login. Cerrar la ventana con la X NO emite esta señal (eso
  // termina la app).
  void cerrarSesion();

private slots:
  void on_btnAgregar_clicked();
  void on_btnEditar_clicked();
  void on_btnRefrescar_clicked();
  void editarFila(const QModelIndex &index);

private:
  // Abre el diálogo de administradores (acción del menú de ajustes).
  void abrirAdministradores();
  // Guarda el tema elegido y lo reaplica en toda la app.
  void cambiarTema(bool oscuro);
  // Construye el menú de ajustes y marca el tema activo.
  void construirMenuAjustes();
  void refrescarTabla();

  Ui::MainWindow *ui;
  StudentTableModel *m_model;
};

#endif // MAINWINDOW_H