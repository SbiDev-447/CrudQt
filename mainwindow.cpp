#include "mainwindow.h"
#include "studentdialog.h"
#include "ui_mainwindow.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlRecord>

QVariant StudentTableModel::data(const QModelIndex &index, int role) const {
  // Columna de calificación (índice 7): NULL se muestra como "Sin calificar".
  if (role == Qt::DisplayRole && index.column() == 7) {
    const QVariant valor = QSqlTableModel::data(index, Qt::EditRole);
    if (valor.isNull()) {
      return QStringLiteral("Sin calificar");
    }
    return QString::number(valor.toDouble(), 'f', 1);
  }
  return QSqlTableModel::data(index, role);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      m_model(new StudentTableModel(this)) {
  ui->setupUi(this);

  // Modelo sobre la tabla estudiantes con orden estable por id.
  m_model->setTable("estudiantes");
  m_model->setSort(0, Qt::AscendingOrder); // id: no cambia al editar filas
  m_model->setHeaderData(0, Qt::Horizontal, QStringLiteral("ID"));
  m_model->setHeaderData(1, Qt::Horizontal, QStringLiteral("Nombre"));
  m_model->setHeaderData(2, Qt::Horizontal, QStringLiteral("Apellido"));
  m_model->setHeaderData(3, Qt::Horizontal, QStringLiteral("Cédula"));
  m_model->setHeaderData(4, Qt::Horizontal, QStringLiteral("Trayecto"));
  m_model->setHeaderData(5, Qt::Horizontal, QStringLiteral("Tramo"));
  m_model->setHeaderData(6, Qt::Horizontal, QStringLiteral("Sección"));
  m_model->setHeaderData(7, Qt::Horizontal, QStringLiteral("Calificación"));

  // La vista solo consulta; crear/editar se hace desde los diálogos.
  ui->tableView->setModel(m_model);
  ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
  ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  ui->tableView->setAlternatingRowColors(true);
  ui->tableView->horizontalHeader()->setStretchLastSection(true);
  ui->tableView->verticalHeader()->setVisible(false);

  connect(ui->tableView, &QTableView::doubleClicked, this,
          &MainWindow::editarFila);

  refrescarTabla();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::refrescarTabla() {
  if (!m_model->select()) {
    QMessageBox::critical(this, "Error de BD", m_model->lastError().text());
    return;
  }
  ui->tableView->resizeColumnsToContents();
  statusBar()->showMessage(
      QStringLiteral("Estudiantes: %1").arg(m_model->rowCount()));
}

void MainWindow::on_btnAgregar_clicked() {
  StudentDialog dialogo(this);
  if (dialogo.exec() == QDialog::Accepted) {
    refrescarTabla();
  }
}

void MainWindow::on_btnEditar_clicked() {
  const QModelIndex actual = ui->tableView->currentIndex();
  if (!actual.isValid()) {
    QMessageBox::information(this, "Editar estudiante",
                             "Selecciona un estudiante de la tabla.");
    return;
  }
  editarFila(actual);
}

void MainWindow::on_btnAdministradores_clicked() {
  // Integrado en la etapa de gestión de administradores.
  QMessageBox::information(this, "Administradores",
                           "La gestión de administradores se integrará en la "
                           "siguiente etapa.");
}

void MainWindow::on_btnRefrescar_clicked() { refrescarTabla(); }

void MainWindow::editarFila(const QModelIndex &index) {
  const QSqlRecord rec = m_model->record(index.row());
  StudentDialog dialogo(this);
  dialogo.setEditData(rec.value("id").toInt(), rec.value("nombre").toString(),
                      rec.value("apellido").toString(),
                      rec.value("cedula").toString(),
                      rec.value("trayecto").toString(),
                      rec.value("tramo").toString(),
                      rec.value("seccion").toString(),
                      rec.value("calificacion"));
  if (dialogo.exec() == QDialog::Accepted) {
    refrescarTabla();
  }
}