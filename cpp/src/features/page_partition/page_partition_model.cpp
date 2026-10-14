#include "features/page_partition/page_partition_model.h"
#include <utility>
PagePartitionModel::PagePartitionModel(PagePartitionConfig config) : config_(std::move(config)) {}
void PagePartitionModel::setConfig(const PagePartitionConfig &config)
{
    config_ = config;
}
void PagePartitionModel::setDefaultWidth(int width)
{
    config_.defaultWidth = width;
}
void PagePartitionModel::setOnlyShowWhenEveDetected(bool enabled)
{
    config_.onlyShowWhenEveDetected = enabled;
}
void PagePartitionModel::setDefaultColor(const QColor &color)
{
    config_.defaultColor = color;
}
void PagePartitionModel::setSelectedColor(const QColor &color)
{
    config_.selectedColor = color;
}
void PagePartitionModel::toggleVisibility()
{
    config_.visible = !config_.visible;
}
void PagePartitionModel::pushHistory()
{
    history_.append(config_.lines);
    while (history_.size() > 20) {
        history_.removeFirst();
    }
}
void PagePartitionModel::addLine(PagePartitionLine line)
{
    pushHistory();
    config_.lines.append(std::move(line));
}
void PagePartitionModel::updateLine(int index, PagePartitionLine line)
{
    if (index < 0 || index >= config_.lines.size()) {
        return;
    }
    pushHistory();
    config_.lines[index] = std::move(line);
}
void PagePartitionModel::removeLine(int index)
{
    if (index < 0 || index >= config_.lines.size()) {
        return;
    }
    pushHistory();
    config_.lines.removeAt(index);
}
void PagePartitionModel::clearLines()
{
    if (config_.lines.isEmpty()) {
        return;
    }
    pushHistory();
    config_.lines.clear();
}
void PagePartitionModel::moveLine(int source, int target)
{
    if (source < 0 || target < 0 || source >= config_.lines.size() ||
        target >= config_.lines.size() || source == target) {
        return;
    }
    pushHistory();
    const auto line = config_.lines.takeAt(source);
    config_.lines.insert(target, line);
}
bool PagePartitionModel::undo()
{
    if (!canUndo()) {
        return false;
    }
    config_.lines = history_.takeLast();
    return true;
}
