#ifndef ROWACTIONDELEGATE_H
#define ROWACTIONDELEGATE_H

#include <QRect>
#include <QSize>
#include <QStyledItemDelegate>

// Delegado de la columna de acciones: pinta un botón "Editar" únicamente en la
// fila seleccionada y emite editRequested() al hacer clic sobre él.
class RowActionDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit RowActionDelegate(QObject *parent = nullptr);

  // Pinta el botón "Editar" solo cuando la fila está seleccionada; en el resto
  // deja la celda vacía (sin botón).
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;

  // Detecta el clic izquierdo sobre el botón (solo al soltar el botón, para no
  // interferir con la selección ni con el doble clic que abre la edición).
  bool editorEvent(QEvent *event, QAbstractItemModel *model,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

  // Tamaño mínimo del botón: QHeaderView::ResizeToContents lo usa para fijar
  // el ancho compacto de la columna de acciones.
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;

signals:
  // Se emite al hacer clic en el botón "Editar" de la fila indicada.
  void editRequested(const QModelIndex &index);

private:
  // Geometría del botón dentro de la celda; compartida por paint() y
  // editorEvent() para que el área de clic coincida con lo dibujado.
  QRect botonRect(const QRect &celda) const;
};

#endif // ROWACTIONDELEGATE_H