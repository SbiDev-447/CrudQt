#ifndef ADMDIALOG_H
#define ADMDIALOG_H

#include <QDialog>

namespace Ui {
class AdminDialog;
}

// Gestión de administradores: agregar usuarios con rol 'admin' y eliminar
// cualquier admin excepto el 'principal' (protegido en UI y en el DELETE).
class AdminDialog : public QDialog {
  Q_OBJECT

public:
  explicit AdminDialog(QWidget *parent = nullptr);
  ~AdminDialog();

private slots:
  void on_btnAgregar_clicked();
  void on_btnEliminar_clicked();
  void on_btnCerrar_clicked();
  void on_tableWidget_itemSelectionChanged();

private:
  void refrescarLista();
  void actualizarEstadoEliminar();

  Ui::AdminDialog *ui;
};

#endif // ADMDIALOG_H