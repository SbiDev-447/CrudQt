#include "studentdialog.h"
#include "ui_studentdialog.h"

#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>

namespace {

// Mensaje claro ante fallo de escritura: la cédula duplicada es el caso
// esperado (la constraint UNIQUE actúa como red de seguridad final).
void mostrarErrorEscritura(QWidget *parent, const QSqlError &error,
                           const QString &cedula) {
  if (error.text().contains("UNIQUE", Qt::CaseInsensitive)) {
    QMessageBox::warning(
        parent, "Cédula duplicada",
        QStringLiteral("Ya existe un estudiante con la cédula %1.").arg(cedula));
    return;
  }
  QMessageBox::critical(parent, "Error de BD", error.text());
}

} // namespace

StudentDialog::StudentDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::StudentDialog) {
  ui->setupUi(this);
  ui->lineNombre->setFocus();
}

StudentDialog::~StudentDialog() { delete ui; }

void StudentDialog::setEditData(int id, const QString &nombre,
                                const QString &apellido, const QString &cedula,
                                const QString &trayecto, const QString &tramo,
                                const QString &seccion,
                                const QVariant &calificacion) {
  m_id = id;
  setWindowTitle(QStringLiteral("Editar estudiante"));
  ui->lineNombre->setText(nombre);
  ui->lineApellido->setText(apellido);
  ui->lineCedula->setText(cedula);
  // Tramo se reajusta al seleccionar trayecto, por eso va después.
  ui->comboBoxTrayecto->setCurrentText(trayecto);
  ui->comboBoxTramo->setCurrentText(tramo);
  ui->comboBoxSeccion->setCurrentText(seccion);
  if (calificacion.isNull()) {
    ui->checkSinCalificar->setChecked(true);
  } else {
    ui->checkSinCalificar->setChecked(false);
    ui->spinCalificacion->setValue(calificacion.toDouble());
  }
  ui->lineNombre->setFocus();
  ui->lineNombre->selectAll();
}

bool StudentDialog::guardar() {
  const QString nombre = ui->lineNombre->text().trimmed();
  const QString apellido = ui->lineApellido->text().trimmed();
  const QString cedula = ui->lineCedula->text().trimmed();

  // Campos obligatorios: aviso claro y foco en el campo faltante.
  if (nombre.isEmpty()) {
    QMessageBox::warning(this, "Campo vacío", "El nombre es obligatorio.");
    ui->lineNombre->setFocus();
    return false;
  }
  if (apellido.isEmpty()) {
    QMessageBox::warning(this, "Campo vacío", "El apellido es obligatorio.");
    ui->lineApellido->setFocus();
    return false;
  }
  if (cedula.isEmpty()) {
    QMessageBox::warning(this, "Campo vacío", "La cédula es obligatoria.");
    ui->lineCedula->setFocus();
    return false;
  }

  // Cédula duplicada: consulta previa; el UNIQUE de la BD es la red final.
  QSqlQuery dup;
  dup.prepare("SELECT id FROM estudiantes WHERE cedula = :c AND id != :id");
  dup.bindValue(":c", cedula);
  dup.bindValue(":id", m_id); // -1 en modo agregar: nunca coincide
  if (!dup.exec()) {
    QMessageBox::critical(this, "Error de BD", dup.lastError().text());
    return false;
  }
  if (dup.next()) {
    QMessageBox::warning(
        this, "Cédula duplicada",
        QStringLiteral("Ya existe un estudiante con la cédula %1.").arg(cedula));
    ui->lineCedula->setFocus();
    ui->lineCedula->selectAll();
    return false;
  }

  // Calificación: NULL cuando se marca "Sin calificar".
  QVariant calificacion;
  if (!ui->checkSinCalificar->isChecked()) {
    calificacion = ui->spinCalificacion->value();
  }

  bool ok = false;
  if (m_id < 0) {
    QSqlQuery insert;
    insert.prepare("INSERT INTO estudiantes (nombre, apellido, cedula, "
                   "trayecto, tramo, seccion, calificacion) "
                   "VALUES (:nombre, :apellido, :cedula, :trayecto, :tramo, "
                   ":seccion, :calificacion)");
    insert.bindValue(":nombre", nombre);
    insert.bindValue(":apellido", apellido);
    insert.bindValue(":cedula", cedula);
    insert.bindValue(":trayecto", ui->comboBoxTrayecto->currentText());
    insert.bindValue(":tramo", ui->comboBoxTramo->currentText());
    insert.bindValue(":seccion", ui->comboBoxSeccion->currentText());
    insert.bindValue(":calificacion", calificacion);
    ok = insert.exec();
    if (!ok) {
      mostrarErrorEscritura(this, insert.lastError(), cedula);
    }
  } else {
    QSqlQuery update;
    update.prepare("UPDATE estudiantes SET nombre = :nombre, "
                   "apellido = :apellido, cedula = :cedula, "
                   "trayecto = :trayecto, tramo = :tramo, "
                   "seccion = :seccion, calificacion = :calificacion "
                   "WHERE id = :id");
    update.bindValue(":nombre", nombre);
    update.bindValue(":apellido", apellido);
    update.bindValue(":cedula", cedula);
    update.bindValue(":trayecto", ui->comboBoxTrayecto->currentText());
    update.bindValue(":tramo", ui->comboBoxTramo->currentText());
    update.bindValue(":seccion", ui->comboBoxSeccion->currentText());
    update.bindValue(":calificacion", calificacion);
    update.bindValue(":id", m_id);
    ok = update.exec();
    if (!ok) {
      mostrarErrorEscritura(this, update.lastError(), cedula);
    } else if (update.numRowsAffected() == 0) {
      // exec() devuelve true aunque no se haya afectado ninguna fila: sin esta
      // comprobación el diálogo se cerraría aparentando un guardado correcto.
      QMessageBox::warning(this, "No se pudo guardar",
                            "El estudiante ya no existe. Puede que se haya "
                            "eliminado en otra ventana.");
      return false;
    }
  }
  return ok;
}

void StudentDialog::on_btnGuardar_clicked() {
  if (guardar()) {
    accept();
  }
}

void StudentDialog::on_btnCancelar_clicked() { reject(); }

void StudentDialog::on_checkSinCalificar_toggled(bool checked) {
  ui->spinCalificacion->setEnabled(!checked);
}

void StudentDialog::on_comboBoxTrayecto_currentIndexChanged(int index) {
  Q_UNUSED(index);
  // Los tramos 1 y 2 aplican a todos los trayectos; se reinicia a "1".
  ui->comboBoxTramo->setCurrentIndex(0);
}