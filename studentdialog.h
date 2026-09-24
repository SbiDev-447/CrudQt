#ifndef STUDENTDIALOG_H
#define STUDENTDIALOG_H

#include <QDialog>
#include <QVariant>

namespace Ui {
class StudentDialog;
}

// Diálogo de alta/edición de estudiantes. En modo agregar (constructor por
// defecto) m_id queda en -1; setEditData() lo cambia a modo edición.
class StudentDialog : public QDialog {
  Q_OBJECT

public:
  explicit StudentDialog(QWidget *parent = nullptr);
  ~StudentDialog();

  void setEditData(int id, const QString &nombre, const QString &apellido,
                   const QString &cedula, const QString &trayecto,
                   const QString &tramo, const QString &seccion,
                   const QVariant &calificacion);

private slots:
  void on_btnGuardar_clicked();
  void on_btnCancelar_clicked();
  void on_checkSinCalificar_toggled(bool checked);
  void on_comboBoxTrayecto_currentIndexChanged(int index);

private:
  bool guardar();

  Ui::StudentDialog *ui;
  int m_id = -1; // -1 = agregar; >= 1 = editar
};

#endif // STUDENTDIALOG_H