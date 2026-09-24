#include "admindialog.h"
#include "database.h"
#include "ui_admindialog.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>

AdminDialog::AdminDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::AdminDialog) {
  ui->setupUi(this);

  // Enter en los campos del formulario acepta según el modo: Agregar en alta,
  // Guardar en edición (ambos pasan por on_btnAgregar_clicked).
  connect(ui->lineUsuario, &QLineEdit::returnPressed, this,
          &AdminDialog::on_btnAgregar_clicked);
  connect(ui->lineClave, &QLineEdit::returnPressed, this,
          &AdminDialog::on_btnAgregar_clicked);

  ui->tableWidget->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  ui->tableWidget->verticalHeader()->setVisible(false);

  refrescarLista();
  ui->lineUsuario->setFocus();
}

AdminDialog::~AdminDialog() { delete ui; }

void AdminDialog::refrescarLista() {
  QSqlQuery query;
  if (!query.exec("SELECT id, usuario, rol FROM usuarios ORDER BY id")) {
    QMessageBox::critical(this, "Error de BD", query.lastError().text());
    return;
  }

  ui->tableWidget->setRowCount(0);
  while (query.next()) {
    const int fila = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(fila);

    auto *itemUsuario = new QTableWidgetItem(query.value(1).toString());
    itemUsuario->setData(Qt::UserRole, query.value(0)); // id para eliminar
    ui->tableWidget->setItem(fila, 0, itemUsuario);

    auto *itemRol = new QTableWidgetItem(query.value(2).toString());
    ui->tableWidget->setItem(fila, 1, itemRol);
  }
  actualizarEstadoBotones();
}

void AdminDialog::actualizarEstadoBotones() {
  // En modo edición se bloquean las acciones sobre la tabla; fuera de él,
  // Editar/Eliminar solo se habilitan sobre un admin que no sea el principal.
  const bool edicionActiva = (m_idEdicion >= 1);
  bool accionable = false;
  const QList<QTableWidgetItem *> seleccion = ui->tableWidget->selectedItems();
  if (!seleccion.isEmpty() && !edicionActiva) {
    const QTableWidgetItem *itemRol =
        ui->tableWidget->item(seleccion.first()->row(), 1);
    if (itemRol && itemRol->text() != QStringLiteral("principal")) {
      accionable = true;
    }
  }
  ui->btnEditar->setEnabled(accionable);
  ui->btnEliminar->setEnabled(accionable);
}

void AdminDialog::on_btnAgregar_clicked() {
  // En modo edición el botón dice "Guardar" y actualiza el admin cargado.
  if (m_idEdicion >= 1) {
    guardarEdicion();
    return;
  }

  const QString usuario = ui->lineUsuario->text().trimmed();
  const QString clave = ui->lineClave->text();

  if (usuario.isEmpty()) {
    QMessageBox::warning(this, "Campo vacío", "El usuario es obligatorio.");
    ui->lineUsuario->setFocus();
    return;
  }
  if (clave.isEmpty()) {
    QMessageBox::warning(this, "Campo vacío",
                         "La contraseña es obligatoria.");
    ui->lineClave->setFocus();
    return;
  }

  // Usuario duplicado: consulta previa; el UNIQUE de la BD es la red final.
  QSqlQuery dup;
  dup.prepare("SELECT id FROM usuarios WHERE usuario = :u");
  dup.bindValue(":u", usuario);
  if (!dup.exec()) {
    QMessageBox::critical(this, "Error de BD", dup.lastError().text());
    return;
  }
  if (dup.next()) {
    QMessageBox::warning(
        this, "Usuario duplicado",
        QStringLiteral("El usuario %1 ya existe.").arg(usuario));
    ui->lineUsuario->setFocus();
    ui->lineUsuario->selectAll();
    return;
  }

  // Nuevo admin con password hasheada igual que el login.
  QSqlQuery insert;
  insert.prepare("INSERT INTO usuarios (usuario, password, rol) "
                 "VALUES (:u, :p, 'admin')");
  insert.bindValue(":u", usuario);
  insert.bindValue(":p", hashPassword(clave));
  if (!insert.exec()) {
    if (insert.lastError().text().contains("UNIQUE", Qt::CaseInsensitive)) {
      QMessageBox::warning(
          this, "Usuario duplicado",
          QStringLiteral("El usuario %1 ya existe.").arg(usuario));
      ui->lineUsuario->setFocus();
      ui->lineUsuario->selectAll();
    } else {
      QMessageBox::critical(this, "Error de BD", insert.lastError().text());
    }
    return;
  }

  refrescarLista();
  ui->lineUsuario->clear();
  ui->lineClave->clear();
  ui->lineUsuario->setFocus();
}

void AdminDialog::iniciarModoEdicion() {
  const QList<QTableWidgetItem *> seleccion = ui->tableWidget->selectedItems();
  if (seleccion.isEmpty()) {
    return;
  }
  const int fila = seleccion.first()->row();
  const QTableWidgetItem *itemRol = ui->tableWidget->item(fila, 1);
  // Doble blindaje en UI: el principal no entra en modo edición.
  if (!itemRol || itemRol->text() == QStringLiteral("principal")) {
    return;
  }

  m_idEdicion = ui->tableWidget->item(fila, 0)->data(Qt::UserRole).toInt();
  ui->lineUsuario->setText(ui->tableWidget->item(fila, 0)->text());
  ui->lineClave->clear();
  // Contraseña vacía en edición = se conserva la actual.
  ui->lineClave->setPlaceholderText(QStringLiteral("Nueva contraseña"));
  ui->btnAgregar->setText(QStringLiteral("Guardar"));
  ui->btnCancelarEdicion->setVisible(true);
  ui->labelAyuda->setText(
      QStringLiteral("Modo edición: la contraseña vacía se conserva. El "
                     "administrador principal no se puede eliminar ni editar."));
  actualizarEstadoBotones();
  ui->lineUsuario->setFocus();
  ui->lineUsuario->selectAll();
}

void AdminDialog::volverAModoAlta() {
  m_idEdicion = -1;
  ui->lineUsuario->clear();
  ui->lineClave->clear();
  ui->lineClave->setPlaceholderText(QStringLiteral("Contraseña"));
  ui->btnAgregar->setText(QStringLiteral("Agregar"));
  ui->btnCancelarEdicion->setVisible(false);
  ui->labelAyuda->setText(
      QStringLiteral("El administrador principal no se puede eliminar ni "
                     "editar. En modo edición, la contraseña vacía se "
                     "conserva."));
  actualizarEstadoBotones();
  ui->lineUsuario->setFocus();
}

bool AdminDialog::guardarEdicion() {
  const QString usuario = ui->lineUsuario->text().trimmed();
  const QString clave = ui->lineClave->text();

  if (usuario.isEmpty()) {
    QMessageBox::warning(this, "Campo vacío", "El usuario es obligatorio.");
    ui->lineUsuario->setFocus();
    return false;
  }

  // Usuario duplicado excluyendo el id propio; el UNIQUE de la BD es la red.
  QSqlQuery dup;
  dup.prepare("SELECT id FROM usuarios WHERE usuario = :u AND id != :id");
  dup.bindValue(":u", usuario);
  dup.bindValue(":id", m_idEdicion);
  if (!dup.exec()) {
    QMessageBox::critical(this, "Error de BD", dup.lastError().text());
    return false;
  }
  if (dup.next()) {
    QMessageBox::warning(
        this, "Usuario duplicado",
        QStringLiteral("El usuario %1 ya existe.").arg(usuario));
    ui->lineUsuario->setFocus();
    ui->lineUsuario->selectAll();
    return false;
  }

  // Contraseña vacía = conservar la actual (no se toca el campo password).
  QString sql =
      QStringLiteral("UPDATE usuarios SET usuario = :u WHERE id = :id "
                     "AND rol != 'principal'");
  if (!clave.isEmpty()) {
    sql = QStringLiteral("UPDATE usuarios SET usuario = :u, password = :p "
                         "WHERE id = :id AND rol != 'principal'");
  }

  QSqlQuery upd;
  upd.prepare(sql);
  upd.bindValue(":u", usuario);
  if (!clave.isEmpty()) {
    upd.bindValue(":p", hashPassword(clave));
  }
  upd.bindValue(":id", m_idEdicion);
  if (!upd.exec()) {
    if (upd.lastError().text().contains("UNIQUE", Qt::CaseInsensitive)) {
      QMessageBox::warning(
          this, "Usuario duplicado",
          QStringLiteral("El usuario %1 ya existe.").arg(usuario));
      ui->lineUsuario->setFocus();
      ui->lineUsuario->selectAll();
    } else {
      QMessageBox::critical(this, "Error de BD", upd.lastError().text());
    }
    return false;
  }
  if (upd.numRowsAffected() == 0) {
    // Ninguna fila afectada: era el principal o el id ya no existe. No hay
    // éxito falso: se avisa y se abandona el modo edición.
    QMessageBox::warning(
        this, "Acción no permitida",
        "No se pudo actualizar: el administrador principal no se puede "
        "editar.");
    volverAModoAlta();
    refrescarLista();
    return false;
  }

  volverAModoAlta();
  refrescarLista();
  return true;
}

void AdminDialog::on_btnEditar_clicked() {
  const QList<QTableWidgetItem *> seleccion = ui->tableWidget->selectedItems();
  if (seleccion.isEmpty()) {
    return;
  }
  const int fila = seleccion.first()->row();
  const QString rol = ui->tableWidget->item(fila, 1)->text();
  if (rol == QStringLiteral("principal")) {
    QMessageBox::warning(this, "Acción no permitida",
                         "El administrador principal no se puede editar.");
    actualizarEstadoBotones();
    return;
  }
  iniciarModoEdicion();
}

void AdminDialog::on_btnCancelarEdicion_clicked() { volverAModoAlta(); }

void AdminDialog::on_btnEliminar_clicked() {
  const QList<QTableWidgetItem *> seleccion = ui->tableWidget->selectedItems();
  if (seleccion.isEmpty()) {
    return;
  }

  const int fila = seleccion.first()->row();
  const QString usuario = ui->tableWidget->item(fila, 0)->text();
  const QString rol = ui->tableWidget->item(fila, 1)->text();

  // Blindaje en UI: el principal nunca se elimina.
  if (rol == QStringLiteral("principal")) {
    QMessageBox::warning(this, "Acción no permitida",
                         "El administrador principal no se puede eliminar.");
    actualizarEstadoBotones();
    return;
  }

  const auto decision = QMessageBox::question(
      this, "Confirmar eliminación",
      QStringLiteral("¿Eliminar al administrador %1?").arg(usuario),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (decision != QMessageBox::Yes) {
    return;
  }

  const int id = ui->tableWidget->item(fila, 0)->data(Qt::UserRole).toInt();
  // Blindaje extra en SQL: la condición excluye al principal.
  QSqlQuery del;
  del.prepare("DELETE FROM usuarios WHERE id = :id AND rol != 'principal'");
  del.bindValue(":id", id);
  if (!del.exec()) {
    QMessageBox::critical(this, "Error de BD", del.lastError().text());
    return;
  }
  refrescarLista();
}

void AdminDialog::on_btnCerrar_clicked() { accept(); }

void AdminDialog::on_tableWidget_itemSelectionChanged() {
  actualizarEstadoBotones();
}