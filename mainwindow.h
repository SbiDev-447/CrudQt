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
  QVariant data(const QModelIndex &index, int role) const override;
};

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void on_btnAgregar_clicked();
  void on_btnEditar_clicked();
  void on_btnAdministradores_clicked();
  void on_btnRefrescar_clicked();
  void editarFila(const QModelIndex &index);

private:
  void refrescarTabla();

  Ui::MainWindow *ui;
  StudentTableModel *m_model;
};

#endif // MAINWINDOW_H