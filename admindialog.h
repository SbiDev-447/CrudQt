#ifndef ADMDIALOG_H
#define ADMDIALOG_H

#include <QDialog>

namespace Ui {
class AdminDialog;
}

// Gestión de administradores: alta/edición de usuarios con rol 'admin' y
// eliminación de cualquier admin excepto el 'principal' (protegido en UI, en
// el UPDATE y en el DELETE). El formulario superior alterna entre modo alta
// (Agregar) y modo edición (Guardar/Cancelar).
class AdminDialog : public QDialog {
  Q_OBJECT

public:
  explicit AdminDialog(QWidget *parent = nullptr);
  ~AdminDialog();

private slots:
  void on_btnAgregar_clicked();
  void on_btnEditar_clicked();
  void on_btnEliminar_clicked();
  void on_btnCancelarEdicion_clicked();
  void on_btnCerrar_clicked();
  void on_tableWidget_itemSelectionChanged();

private:
  void refrescarLista();
  void actualizarEstadoBotones();
  void iniciarModoEdicion();
  void volverAModoAlta();
  bool guardarEdicion();

  Ui::AdminDialog *ui;
  int m_idEdicion = -1; // -1 = modo alta; >= 1 = editando ese id
};

#endif // ADMDIALOG_H