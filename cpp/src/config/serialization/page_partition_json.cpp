#include "config/serialization/page_partition_json.h"
#include <QUuid>

namespace ConfigSerialization
{
namespace
{
QPoint parsePoint(const QJsonValue& value)
{
    const QJsonArray coordinates = value.toArray();
    if (coordinates.size() != 2) {
        return {};
    }
    return {coordinates.at(0).toInt(), coordinates.at(1).toInt()};
}
QJsonObject pagePartitionLineToJson(const PagePartitionLine& line)
{
    return {
        {QStringLiteral("id"), line.id},
        {QStringLiteral("type"),
         line.isHorizontal() ? QStringLiteral("horizontal") : QStringLiteral("vertical")},
        {QStringLiteral("start"), QJsonArray{line.start.x(), line.start.y()}},
        {QStringLiteral("end"), QJsonArray{line.end.x(), line.end.y()}},
        {QStringLiteral("color"), line.color.name()},
        {QStringLiteral("width"), line.width},
        {QStringLiteral("note"), line.note},
    };
}
} // namespace

PagePartitionConfig parsePagePartitionConfig(const QJsonObject& object)
{
    PagePartitionConfig config;
    const QColor defaultColor(object.value(QStringLiteral("defaultColor")).toString());
    const QColor selectedColor(object.value(QStringLiteral("selectedColor")).toString());
    if (defaultColor.isValid()) {
        config.defaultColor = defaultColor;
    }
    if (selectedColor.isValid()) {
        config.selectedColor = selectedColor;
    }
    config.defaultWidth = qBound(kPagePartitionLineWidthMinimum,
                                 object.value(QStringLiteral("defaultWidth")).toInt(2),
                                 kPagePartitionLineWidthMaximum);
    config.visible = object.value(QStringLiteral("visible")).toBool(true);
    config.onlyShowWhenEveDetected =
        object.value(QStringLiteral("onlyShowWhenEveDetected")).toBool(false);

    for (const QJsonValue& value : object.value(QStringLiteral("lines")).toArray()) {
        const QJsonObject lineObject = value.toObject();
        PagePartitionLine line;
        line.id = lineObject.value(QStringLiteral("id")).toString().trimmed();
        if (line.id.isEmpty()) {
            line.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        line.start = parsePoint(lineObject.value(QStringLiteral("start")));
        line.end = parsePoint(lineObject.value(QStringLiteral("end")));
        const QString type = lineObject.value(QStringLiteral("type")).toString();
        const int dx = qAbs(line.end.x() - line.start.x());
        const int dy = qAbs(line.end.y() - line.start.y());
        if (type == QStringLiteral("vertical") || (type.isEmpty() && dy > dx)) {
            line.end.setX(line.start.x());
        } else {
            line.end.setY(line.start.y());
        }
        if (line.start == line.end) {
            continue;
        }
        const QColor color(lineObject.value(QStringLiteral("color")).toString());
        line.color = color.isValid() ? color : config.defaultColor;
        line.width = qBound(kPagePartitionLineWidthMinimum,
                            lineObject.value(QStringLiteral("width")).toInt(config.defaultWidth),
                            kPagePartitionLineWidthMaximum);
        line.note = lineObject.value(QStringLiteral("note")).toString().trimmed();
        config.lines.append(std::move(line));
    }
    return config;
}

QJsonObject pagePartitionConfigToJson(const PagePartitionConfig& config)
{
    QJsonArray lines;
    for (const auto& line : config.lines) {
        lines.append(pagePartitionLineToJson(line));
    }
    return {
        {QStringLiteral("version"), 1},
        {QStringLiteral("defaultColor"), config.defaultColor.name()},
        {QStringLiteral("selectedColor"), config.selectedColor.name()},
        {QStringLiteral("defaultWidth"), config.defaultWidth},
        {QStringLiteral("visible"), config.visible},
        {QStringLiteral("onlyShowWhenEveDetected"), config.onlyShowWhenEveDetected},
        {QStringLiteral("lines"), lines},
    };
}
} // namespace ConfigSerialization
