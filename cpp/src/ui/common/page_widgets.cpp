#include "ui/common/page_widgets.h"
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
namespace PageWidgets
{
QLabel *makeHeading(const QString &text)
{
    auto *heading = new QLabel(text);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    return heading;
}

QLabel *makeDescription(const QString &text)
{
    auto *description = new QLabel(text);
    description->setObjectName(QStringLiteral("muted"));
    description->setWordWrap(true);
    return description;
}

void configureTable(QTableWidget *table)
{
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(36);
    table->horizontalHeader()->setStretchLastSection(true);
}

QTableWidgetItem *item(const QString &text)
{
    auto *result = new QTableWidgetItem(text);
    result->setToolTip(text);
    return result;
}

} // namespace PageWidgets
