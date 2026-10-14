#pragma once
#include <QString>
class QLabel;
class QTableWidget;
class QTableWidgetItem;
namespace PageWidgets
{
QLabel *makeHeading(const QString &text);
QLabel *makeDescription(const QString &text);
void configureTable(QTableWidget *table);
QTableWidgetItem *item(const QString &text);
} // namespace PageWidgets
