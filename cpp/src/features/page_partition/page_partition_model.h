#pragma once
#include "config/config_types.h"
class PagePartitionModel
{
  public:
    explicit PagePartitionModel(PagePartitionConfig config = {});
    const PagePartitionConfig &config() const noexcept { return config_; }
    void setConfig(const PagePartitionConfig &config);
    void setDefaultWidth(int width);
    void setOnlyShowWhenEveDetected(bool enabled);
    void setDefaultColor(const QColor &color);
    void setSelectedColor(const QColor &color);
    void toggleVisibility();
    void addLine(PagePartitionLine line);
    void updateLine(int index, PagePartitionLine line);
    void removeLine(int index);
    void clearLines();
    void moveLine(int source, int target);
    bool undo();
    bool canUndo() const noexcept { return !history_.isEmpty(); }

  private:
    void pushHistory();
    PagePartitionConfig config_;
    QList<QList<PagePartitionLine>> history_;
};
