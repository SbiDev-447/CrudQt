#include "mainwindow.h"
#include "admindialog.h"
#include "rowactiondelegate.h"
#include "studentdialog.h"
#include "ui_mainwindow.h"

#include <QColor>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlRecord>

int StudentTableModel::columnCount(const QModelIndex &parent) const {
  // Columna virtual de acciones al final; el SQL de la tabla no cambia.
  return QSqlTableModel::columnCount(parent) + 1;
}

QVariant StudentTableModel::data(const QModelIndex &index, int role) const {
  // La columna virtual de acciones no tiene datos: la pinta el delegado.
  if (index.column() == columnCount(QModelIndex()) - 1) {
    return QVariant();
  }
  // Columna de calificación (índice 7): NULL se muestra como "Sin calificar".
  if (index.column() == 7) {
    const QVariant valor = QSqlTableModel::data(index, Qt::EditRole);
    if (role == Qt::DisplayRole) {
      if (valor.isNull()) {
        return QStringLiteral("Sin calificar");
      }
      return QString::number(valor.toDouble(), 'f', 1);
    }
    // "Sin calificar" es un placeholder, no una nota: se atenúa con fg2 de
    // Gruvbox (contraste 8.6:1, cumple WCAG AA; el gray oficial #928374 queda
    // por debajo del 4.5:1 sobre el fondo). Con la fila seleccionada manda el
    // color del QSS (QTableView::item:selected).
    if (role == Qt::ForegroundRole && valor.isNull()) {
      return QColor(QStringLiteral("#d5c4a1"));
    }
  }
  return QSqlTableModel::data(index, role);
}

QVariant StudentTableModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const {
  // Encabezado de la columna virtual de acciones (solo presentación).
  if (orientation == Qt::Horizontal &&
      section == columnCount(QModelIndex()) - 1) {
    if (role == Qt::DisplayRole) {
      return QStringLiteral("Acciones");
    }
    return QVariant();
  }
  return QSqlTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags StudentTableModel::flags(const QModelIndex &index) const {
  // La columna virtual es solo presentación: no editable (el clic lo maneja el
  // delegado), pero sí seleccionable para que la fila entera se seleccione.
  if (index.column() == columnCount(QModelIndex()) - 1) {
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  }
  return QSqlTableModel::flags(index);
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
  ui->tableView->verticalHeader()->setVisible(false);

  // Las columnas reparten el ancho disponible sin huecos muertos: nombre,
  // apellido, cédula y sección estiran; ID y calificación conservan su ancho
  // natural; trayecto/tramo quedan en tamaño razonable (ajustable); la columna
  // de acciones (botón Editar) queda compacta al final, sin estirarse.
  QHeaderView *header = ui->tableView->horizontalHeader();
  header->setStretchLastSection(false);
  header->setSectionResizeMode(0, QHeaderView::ResizeToContents); // ID
  header->setSectionResizeMode(1, QHeaderView::Stretch);          // Nombre
  header->setSectionResizeMode(2, QHeaderView::Stretch);          // Apellido
  header->setSectionResizeMode(3, QHeaderView::Stretch);          // Cédula
  header->setSectionResizeMode(4, QHeaderView::Interactive);      // Trayecto
  header->setSectionResizeMode(5, QHeaderView::Interactive);      // Tramo
  header->setSectionResizeMode(6, QHeaderView::Stretch);          // Sección
  header->setSectionResizeMode(7, QHeaderView::ResizeToContents); // Calificación
  header->setSectionResizeMode(8, QHeaderView::ResizeToContents); // Acciones
  header->resizeSection(4, 110); // ancho inicial razonable de Trayecto
  header->resizeSection(5, 80);  // ancho inicial razonable de Tramo

  connect(ui->tableView, &QTableView::doubleClicked, this,
          &MainWindow::editarFila);

  // Botón "Editar" en la columna de acciones, visible solo en la fila
  // seleccionada; el clic reutiliza la misma edición del botón global Editar.
  const int colAcciones = m_model->columnCount() - 1;
  auto *delegadoAcciones = new RowActionDelegate(ui->tableView);
  ui->tableView->setItemDelegateForColumn(colAcciones, delegadoAcciones);
  connect(delegadoAcciones, &RowActionDelegate::editRequested, this,
          &MainWindow::editarFila);

  // Al moverse la selección se repinta la columna de acciones: el botón
  // desaparece de la fila anterior y aparece en la fila nueva.
  connect(ui->tableView->selectionModel(),
          &QItemSelectionModel::currentRowChanged, this,
          [this, colAcciones](const QModelIndex &actual,
                              const QModelIndex &anterior) {
            if (anterior.isValid()) {
              ui->tableView->update(
                  m_model->index(anterior.row(), colAcciones));
            }
            if (actual.isValid()) {
              ui->tableView->update(m_model->index(actual.row(), colAcciones));
            }
          });

  refrescarTabla();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::refrescarTabla() {
  if (!m_model->select()) {
    QMessageBox::critical(this, "Error de BD", m_model->lastError().text());
    return;
  }
  // El header ya reparte el ancho por sección (Stretch/Interactive); no se
  // re-escalan las columnas al refrescar para no anular ese reparto.
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
  AdminDialog dialogo(this);
  dialogo.exec();
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