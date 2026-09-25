#include "rowactiondelegate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QFontMetrics>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionButton>

namespace {
// Margen entre el botón y el borde de la celda.
constexpr int kMargenBoton = 4;
// Padding horizontal del botón además del texto.
constexpr int kPaddingTextoBoton = 20;
} // namespace

RowActionDelegate::RowActionDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

QRect RowActionDelegate::botonRect(const QRect &celda) const {
  return celda.adjusted(kMargenBoton, kMargenBoton, -kMargenBoton,
                        -kMargenBoton);
}

void RowActionDelegate::paint(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const {
  // Fondo normal o de selección; el texto de la celda está vacío.
  QStyledItemDelegate::paint(painter, option, index);

  // Sin selección: celda vacía, sin botón.
  if (!(option.state & QStyle::State_Selected))
    return;

  QStyleOptionButton boton;
  boton.rect = botonRect(option.rect);
  boton.text = QStringLiteral("Editar");
  boton.state = QStyle::State_Enabled;
  // El estilo del widget que pinta la celda (la vista) dibuja el botón.
  QStyle *style = option.widget ? option.widget->style() : QApplication::style();
  style->drawControl(QStyle::CE_PushButton, &boton, painter, option.widget);
}

bool RowActionDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                    const QStyleOptionViewItem &option,
                                    const QModelIndex &index) {
  Q_UNUSED(model);

  // Solo un clic izquierdo completado (botón soltado) dentro del botón.
  if (event->type() != QEvent::MouseButtonRelease) {
    return false;
  }
  const auto *me = static_cast<const QMouseEvent *>(event);
  if (me->button() != Qt::LeftButton) {
    return false;
  }
  if (!index.isValid()) {
    return false;
  }
  if (!botonRect(option.rect).contains(me->position().toPoint())) {
    return false;
  }

  // La vista no entrega State_Selected en editorEvent: se consulta la fila
  // seleccionada directamente. En la práctica el clic ya seleccionó la fila,
  // pero la comprobación protege el caso de que la selección se haya movido.
  const auto *vista = qobject_cast<const QAbstractItemView *>(parent());
  if (!vista || !vista->selectionModel() ||
      !vista->selectionModel()->isRowSelected(index.row(), index.parent())) {
    return false;
  }

  emit editRequested(index);
  return true;
}

QSize RowActionDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const {
  Q_UNUSED(index);
  // Ancho suficiente para el botón "Editar" más los márgenes; así la columna
  // de acciones queda compacta (ResizeToContents) sin recortar el botón.
  const QFontMetrics fm = option.fontMetrics;
  const int ancho = fm.horizontalAdvance(QStringLiteral("Editar")) +
                    2 * kPaddingTextoBoton + 2 * kMargenBoton;
  return QSize(ancho, fm.height() + 2 * kMargenBoton);
}