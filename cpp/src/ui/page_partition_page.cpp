#include "ui/page_partition_page.h"

#include <algorithm>
#include <functional>
#include <optional>

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDropEvent>
#include <QFormLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineF>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

namespace {

inline constexpr int kMinimumLineLength = 12;
inline constexpr int kLineIdRole = Qt::UserRole + 1;

PagePartitionLine constrainedLine(
    QPoint start,
    QPoint cursor,
    const QColor& color,
    int width,
    QString id = {},
    QString note = {})
{
    QPoint end = cursor;
    if (qAbs(cursor.x() - start.x()) >= qAbs(cursor.y() - start.y())) {
        end.setY(start.y());
    } else {
        end.setX(start.x());
    }
    return {
        std::move(id), start, end, color,
        qBound(kPagePartitionLineWidthMinimum, width, kPagePartitionLineWidthMaximum),
        std::move(note)};
}

class PartitionOverlay final : public QWidget {
public:
    explicit PartitionOverlay(const QRect& screenGeometry)
        : QWidget(nullptr,
              Qt::Tool
                  | Qt::FramelessWindowHint
                  | Qt::WindowStaysOnTopHint
                  | Qt::WindowTransparentForInput
                  | Qt::WindowDoesNotAcceptFocus
                  | Qt::NoDropShadowWindowHint)
        , screenGeometry_(screenGeometry)
    {
        setObjectName(QStringLiteral("pagePartitionOverlay"));
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        setGeometry(screenGeometry_);
    }

    void setLines(
        const QList<PagePartitionLine>& lines,
        QString selectedId,
        QColor selectedColor)
    {
        lines_ = lines;
        selectedId_ = std::move(selectedId);
        selectedColor_ = std::move(selectedColor);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QPoint origin = screenGeometry_.topLeft();
        for (const auto& line : lines_) {
            const QColor color = line.id == selectedId_ ? selectedColor_ : line.color;
            QPen pen(color, line.width, Qt::SolidLine, Qt::RoundCap);
            painter.setPen(pen);
            painter.drawLine(line.start - origin, line.end - origin);
        }
    }

private:
    QRect screenGeometry_;
    QList<PagePartitionLine> lines_;
    QString selectedId_;
    QColor selectedColor_;
};

class PartitionTable final : public QTableWidget {
public:
    using QTableWidget::QTableWidget;
    std::function<void(int, int)> moveRequested;

protected:
    void dropEvent(QDropEvent* event) override
    {
        const int sourceRow = currentRow();
        int targetRow = indexAt(event->position().toPoint()).row();
        if (targetRow < 0) {
            targetRow = rowCount() - 1;
        }
        if (sourceRow >= 0 && targetRow >= 0 && sourceRow != targetRow
            && moveRequested) {
            moveRequested(sourceRow, targetRow);
        }
        event->acceptProposedAction();
    }
};

class PartitionDrawOverlay final : public QWidget {
public:
    using ResultCallback = std::function<void(std::optional<PagePartitionLine>)>;

    PartitionDrawOverlay(
        QScreen* screen,
        QColor color,
        int width,
        std::optional<PagePartitionLine> initial,
        ResultCallback callback)
        : QWidget(nullptr,
              Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
        , screenGeometry_(screen->geometry())
        , color_(std::move(color))
        , width_(width)
        , editing_(initial.has_value())
        , preview_(std::move(initial))
        , callback_(std::move(callback))
    {
        setObjectName(QStringLiteral("pagePartitionDrawOverlay"));
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setGeometry(screenGeometry_);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(editing_ ? Qt::ArrowCursor : Qt::CrossCursor);
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        raise();
        activateWindow();
        setFocus(Qt::OtherFocusReason);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(5, 12, 17, 40));
        painter.setRenderHint(QPainter::Antialiasing);
        if (preview_.has_value()) {
            painter.setPen(QPen(color_, width_, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(
                preview_->start - screenGeometry_.topLeft(),
                preview_->end - screenGeometry_.topLeft());
            if (editing_) {
                QPen handlePen(QColor(QStringLiteral("#FFFFFF")));
                handlePen.setWidthF(0.5);
                painter.setPen(handlePen);
                painter.setBrush(QColor(QStringLiteral("#17303C")));
                constexpr qreal handleRadius = 1.5;
                painter.drawEllipse(
                    QPointF(preview_->start - screenGeometry_.topLeft()),
                    handleRadius, handleRadius);
                painter.drawEllipse(
                    QPointF(preview_->end - screenGeometry_.topLeft()),
                    handleRadius, handleRadius);
            }
        }

        const QRect hintRect(24, 24, editing_ ? 440 : 360, 62);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(12, 29, 39, 225));
        painter.drawRoundedRect(hintRect, 8, 8);
        painter.setPen(QColor(QStringLiteral("#E8F0F5")));
        painter.drawText(
            hintRect.adjusted(16, 8, -16, -8),
            Qt::AlignVCenter | Qt::AlignLeft,
            editing_
                ? QStringLiteral("拖动端点调整长度，拖动线身平移\n按 Enter 保存，Esc 取消")
                : QStringLiteral("拖动绘制水平或垂直分区线\n按 Esc 取消"));
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) {
            return;
        }
        if (editing_) {
            dragMode_ = hitTest(event->globalPosition().toPoint());
            if (dragMode_ == DragMode::None) {
                return;
            }
            dragAnchor_ = event->globalPosition().toPoint();
            dragStartLine_ = *preview_;
            if (dragMode_ == DragMode::MoveLine) {
                setCursor(Qt::ClosedHandCursor);
            }
            event->accept();
            return;
        }
        drawing_ = true;
        start_ = event->globalPosition().toPoint();
        preview_ = constrainedLine(start_, start_, color_, width_);
        update();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (editing_) {
            const QPoint globalPosition = event->globalPosition().toPoint();
            if (dragMode_ == DragMode::None) {
                updateHoverCursor(globalPosition);
                return;
            }
            preview_ = editedLine(globalPosition);
            update();
            return;
        }
        if (!drawing_) {
            return;
        }
        preview_ = constrainedLine(
            start_, event->globalPosition().toPoint(), color_, width_);
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (editing_) {
            if (event->button() == Qt::LeftButton
                && dragMode_ != DragMode::None) {
                dragMode_ = DragMode::None;
                updateHoverCursor(event->globalPosition().toPoint());
                event->accept();
            }
            return;
        }
        if (!drawing_ || event->button() != Qt::LeftButton) {
            return;
        }
        drawing_ = false;
        PagePartitionLine result = constrainedLine(
            start_, event->globalPosition().toPoint(), color_, width_);
        const int length = qAbs(result.end.x() - result.start.x())
            + qAbs(result.end.y() - result.start.y());
        if (length < kMinimumLineLength) {
            preview_.reset();
            update();
            return;
        }
        finish(std::move(result));
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Escape) {
            finish(std::nullopt);
            return;
        }
        if (editing_
            && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
            finish(preview_);
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    enum class DragMode {
        None,
        StartHandle,
        EndHandle,
        MoveLine,
    };

    static qreal distanceToSegment(
        const QPointF& point,
        const QPointF& start,
        const QPointF& end)
    {
        const QPointF segment = end - start;
        const qreal lengthSquared = QPointF::dotProduct(segment, segment);
        if (lengthSquared <= 0.0) {
            return QLineF(point, start).length();
        }
        const qreal projection = QPointF::dotProduct(point - start, segment)
            / lengthSquared;
        const QPointF nearest = start
            + segment * qBound<qreal>(0.0, projection, 1.0);
        return QLineF(point, nearest).length();
    }

    DragMode hitTest(const QPoint& globalPosition) const
    {
        if (!preview_.has_value()) {
            return DragMode::None;
        }

        constexpr qreal handleHitRadius = 11.0;
        const QPointF point(globalPosition);
        const qreal startDistance = QLineF(point, QPointF(preview_->start)).length();
        const qreal endDistance = QLineF(point, QPointF(preview_->end)).length();
        const bool startHit = startDistance <= handleHitRadius;
        const bool endHit = endDistance <= handleHitRadius;
        if (startHit || endHit) {
            return startHit && (!endHit || startDistance < endDistance)
                ? DragMode::StartHandle
                : DragMode::EndHandle;
        }

        const qreal lineHitTolerance = qMax<qreal>(
            6.0, preview_->width / 2.0 + 3.0);
        return distanceToSegment(
                   point, QPointF(preview_->start), QPointF(preview_->end))
                <= lineHitTolerance
            ? DragMode::MoveLine
            : DragMode::None;
    }

    void updateHoverCursor(const QPoint& globalPosition)
    {
        const DragMode mode = hitTest(globalPosition);
        if (mode == DragMode::MoveLine) {
            setCursor(Qt::OpenHandCursor);
        } else if (mode == DragMode::StartHandle || mode == DragMode::EndHandle) {
            setCursor(preview_->isHorizontal() ? Qt::SizeHorCursor : Qt::SizeVerCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }

    PagePartitionLine editedLine(const QPoint& globalPosition) const
    {
        PagePartitionLine result = dragStartLine_;
        const QPoint delta = globalPosition - dragAnchor_;
        if (dragMode_ == DragMode::MoveLine) {
            const QRect bounds = screenGeometry_;
            const int minX = qMin(result.start.x(), result.end.x());
            const int maxX = qMax(result.start.x(), result.end.x());
            const int minY = qMin(result.start.y(), result.end.y());
            const int maxY = qMax(result.start.y(), result.end.y());
            const int minDeltaX = bounds.left() - minX;
            const int maxDeltaX = bounds.right() - maxX;
            const int minDeltaY = bounds.top() - minY;
            const int maxDeltaY = bounds.bottom() - maxY;
            const int dx = minDeltaX <= maxDeltaX
                ? qBound(minDeltaX, delta.x(), maxDeltaX)
                : 0;
            const int dy = minDeltaY <= maxDeltaY
                ? qBound(minDeltaY, delta.y(), maxDeltaY)
                : 0;
            result.start += QPoint(dx, dy);
            result.end += QPoint(dx, dy);
            return result;
        }

        const bool movingStart = dragMode_ == DragMode::StartHandle;
        const bool horizontal = dragStartLine_.isHorizontal();
        const int originalStart = horizontal
            ? dragStartLine_.start.x()
            : dragStartLine_.start.y();
        const int originalEnd = horizontal
            ? dragStartLine_.end.x()
            : dragStartLine_.end.y();
        const int current = movingStart ? originalStart : originalEnd;
        const int fixed = movingStart ? originalEnd : originalStart;
        const int movement = horizontal ? delta.x() : delta.y();
        const int screenMinimum = horizontal
            ? screenGeometry_.left()
            : screenGeometry_.top();
        const int screenMaximum = horizontal
            ? screenGeometry_.right()
            : screenGeometry_.bottom();
        const bool originalStartIsBeforeEnd = originalStart <= originalEnd;
        const int direction = movingStart
            ? (originalStartIsBeforeEnd ? -1 : 1)
            : (originalStartIsBeforeEnd ? 1 : -1);
        int minimum = screenMinimum;
        int maximum = screenMaximum;
        if (direction < 0) {
            maximum = qMin(maximum, fixed - kMinimumLineLength);
        } else {
            minimum = qMax(minimum, fixed + kMinimumLineLength);
        }
        if (minimum > maximum) {
            return result;
        }

        const int updated = qBound(minimum, current + movement, maximum);
        if (horizontal) {
            (movingStart ? result.start.rx() : result.end.rx()) = updated;
        } else {
            (movingStart ? result.start.ry() : result.end.ry()) = updated;
        }
        return result;
    }

    void finish(std::optional<PagePartitionLine> result)
    {
        if (finished_) {
            return;
        }
        finished_ = true;
        if (callback_) {
            callback_(std::move(result));
        }
        close();
    }

    QRect screenGeometry_;
    QColor color_;
    int width_{};
    bool editing_{false};
    std::optional<PagePartitionLine> preview_;
    ResultCallback callback_;
    QPoint start_;
    QPoint dragAnchor_;
    PagePartitionLine dragStartLine_;
    DragMode dragMode_{DragMode::None};
    bool drawing_{false};
    bool finished_{false};
};

QLabel* makeDescription(const QString& text)
{
    auto* label = new QLabel(text);
    label->setObjectName(QStringLiteral("muted"));
    label->setWordWrap(true);
    return label;
}

QString swatchStyle(const QColor& color)
{
    return QStringLiteral(
               "QPushButton { background: %1; border: 2px solid #6D8A98; color: %2; }")
        .arg(color.name(), color.lightness() > 140 ? QStringLiteral("#071118")
                                                  : QStringLiteral("#FFFFFF"));
}

QColor chooseRgbColor(QWidget* parent, const QColor& initial, const QString& title)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);

    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto makeChannel = [&dialog](const QString& name, int value) {
        auto* spinBox = new QSpinBox(&dialog);
        spinBox->setObjectName(name);
        spinBox->setRange(0, 255);
        spinBox->setValue(value);
        return spinBox;
    };
    auto* red = makeChannel(QStringLiteral("pagePartitionRgbRed"), initial.red());
    auto* green = makeChannel(QStringLiteral("pagePartitionRgbGreen"), initial.green());
    auto* blue = makeChannel(QStringLiteral("pagePartitionRgbBlue"), initial.blue());
    form->addRow(QStringLiteral("红色 (R)"), red);
    form->addRow(QStringLiteral("绿色 (G)"), green);
    form->addRow(QStringLiteral("蓝色 (B)"), blue);
    layout->addLayout(form);

    auto* preview = new QLabel(&dialog);
    preview->setObjectName(QStringLiteral("pagePartitionRgbPreview"));
    preview->setAlignment(Qt::AlignCenter);
    preview->setMinimumHeight(32);
    const auto updatePreview = [red, green, blue, preview] {
        const QColor color(red->value(), green->value(), blue->value());
        preview->setText(color.name().toUpper());
        preview->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: %2; border: 1px solid #6D8A98; }")
                .arg(color.name(), color.lightness() > 140 ? QStringLiteral("#071118")
                                                           : QStringLiteral("#FFFFFF")));
    };
    QObject::connect(red, &QSpinBox::valueChanged, &dialog, updatePreview);
    QObject::connect(green, &QSpinBox::valueChanged, &dialog, updatePreview);
    QObject::connect(blue, &QSpinBox::valueChanged, &dialog, updatePreview);
    updatePreview();
    layout->addWidget(preview);

    auto* pickerButton = new QPushButton(QStringLiteral("从色板选取…"), &dialog);
    QObject::connect(pickerButton, &QPushButton::clicked, &dialog, [&] {
        const QColor selected = QColorDialog::getColor(
            QColor(red->value(), green->value(), blue->value()),
            &dialog, QStringLiteral("选择颜色"));
        if (!selected.isValid()) {
            return;
        }
        red->setValue(selected.red());
        green->setValue(selected.green());
        blue->setValue(selected.blue());
    });
    layout->addWidget(pickerButton, 0, Qt::AlignLeft);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    return QColor(red->value(), green->value(), blue->value(), initial.alpha());
}

} // namespace

PagePartitionPage::PagePartitionPage(PagePartitionConfig config, QWidget* parent)
    : QWidget(parent)
    , model_(std::move(config))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* heading = new QLabel(QStringLiteral("页面分区"), this);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(heading);
    layout->addWidget(makeDescription(QStringLiteral(
        "在任意显示器上绘制置顶、鼠标穿透的辅助线，用于划分客户端页面区域。")));

    auto* settings = new QGroupBox(QStringLiteral("线条设置"), this);
    settings->setObjectName(QStringLiteral("pagePartitionSettingsPanel"));
    auto* settingsLayout = new QHBoxLayout(settings);
    settingsLayout->setContentsMargins(16, 18, 16, 12);
    settingsLayout->addWidget(new QLabel(QStringLiteral("默认粗细"), settings));
    widthSpin_ = new QSpinBox(settings);
    widthSpin_->setObjectName(QStringLiteral("pagePartitionDefaultWidth"));
    widthSpin_->setRange(kPagePartitionLineWidthMinimum, kPagePartitionLineWidthMaximum);
    widthSpin_->setSuffix(QStringLiteral(" px"));
    widthSpin_->setValue(model_.config().defaultWidth);
    settingsLayout->addWidget(widthSpin_);
    defaultColorButton_ = new QPushButton(QStringLiteral("默认颜色"), settings);
    defaultColorButton_->setObjectName(QStringLiteral("pagePartitionDefaultColor"));
    settingsLayout->addWidget(defaultColorButton_);
    selectedColorButton_ = new QPushButton(QStringLiteral("选中颜色"), settings);
    selectedColorButton_->setObjectName(QStringLiteral("pagePartitionSelectedColor"));
    settingsLayout->addWidget(selectedColorButton_);
    onlyShowWhenEveDetectedCheck_ = new QCheckBox(
        QStringLiteral("仅在检测到 EVE 客户端时显示线条"), settings);
    onlyShowWhenEveDetectedCheck_->setObjectName(
        QStringLiteral("pagePartitionOnlyShowWhenEveDetected"));
    onlyShowWhenEveDetectedCheck_->setToolTip(QStringLiteral(
        "启用后，仅在检测到可用的 EVE 客户端窗口时显示分区线条。"));
    onlyShowWhenEveDetectedCheck_->setChecked(model_.config().onlyShowWhenEveDetected);
    settingsLayout->addWidget(onlyShowWhenEveDetectedCheck_);
    settingsLayout->addStretch(1);
    layout->addWidget(settings);

    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("屏幕选择"), this));
    screenCombo_ = new QComboBox(this);
    screenCombo_->setObjectName(QStringLiteral("pagePartitionScreenSelection"));
    screenCombo_->setToolTip(QStringLiteral(
        "指定添加新线条的目标屏幕；重新绘制已有线条时仍在原位置进行。"));
    controls->addWidget(screenCombo_);
    addButton_ = new QPushButton(QStringLiteral("添加新线条"), this);
    addButton_->setObjectName(QStringLiteral("addPagePartitionLine"));
    addButton_->setProperty("accent", true);
    visibilityButton_ = new QPushButton(this);
    visibilityButton_->setObjectName(QStringLiteral("togglePagePartitionVisibility"));
    auto* saveButton = new QPushButton(QStringLiteral("保存"), this);
    saveButton->setObjectName(QStringLiteral("savePagePartition"));
    auto* clearButton = new QPushButton(QStringLiteral("清除全部"), this);
    clearButton->setObjectName(QStringLiteral("clearPagePartitionLines"));
    controls->addWidget(addButton_);
    controls->addWidget(visibilityButton_);
    controls->addWidget(saveButton);
    controls->addWidget(clearButton);
    controls->addStretch(1);
    layout->addLayout(controls);

    auto* actions = new QHBoxLayout;
    auto* clearSelectionButton = new QPushButton(QStringLiteral("取消选中"), this);
    clearSelectionButton->setObjectName(QStringLiteral("clearPagePartitionSelection"));
    redrawButton_ = new QPushButton(QStringLiteral("重新绘制"), this);
    redrawButton_->setObjectName(QStringLiteral("redrawPagePartitionLine"));
    deleteButton_ = new QPushButton(QStringLiteral("删除线条"), this);
    deleteButton_->setObjectName(QStringLiteral("deletePagePartitionLine"));
    undoButton_ = new QPushButton(QStringLiteral("撤销改动"), this);
    undoButton_->setObjectName(QStringLiteral("undoPagePartitionChange"));
    actions->addWidget(clearSelectionButton);
    actions->addWidget(redrawButton_);
    actions->addWidget(deleteButton_);
    actions->addWidget(undoButton_);
    actions->addStretch(1);
    layout->addLayout(actions);

    auto* table = new PartitionTable(0, 6, this);
    table_ = table;
    table_->setObjectName(QStringLiteral("pagePartitionTable"));
    table_->setHorizontalHeaderLabels({
        QStringLiteral("序号"), QStringLiteral("类型"), QStringLiteral("粗细"),
        QStringLiteral("颜色"), QStringLiteral("备注"), QStringLiteral("坐标")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    table_->verticalHeader()->hide();
    table_->setAlternatingRowColors(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setDragEnabled(true);
    table_->setAcceptDrops(true);
    table_->setDragDropMode(QAbstractItemView::InternalMove);
    table_->setDropIndicatorShown(true);
    table->moveRequested = [this](int source, int target) { moveLine(source, target); };
    layout->addWidget(table_, 1);

    statusLabel_ = new QLabel(QStringLiteral("就绪"), this);
    statusLabel_->setObjectName(QStringLiteral("pagePartitionStatus"));
    statusLabel_->setProperty("status", QStringLiteral("muted"));
    layout->addWidget(statusLabel_);

    connect(addButton_, &QPushButton::clicked, this, &PagePartitionPage::addLine);
    connect(visibilityButton_, &QPushButton::clicked, this, &PagePartitionPage::toggleVisibility);
    connect(saveButton, &QPushButton::clicked, this, [this] {
        emit configurationChanged();
        statusLabel_->setText(QStringLiteral("配置已保存"));
    });
    connect(clearButton, &QPushButton::clicked, this, &PagePartitionPage::clearAllLines);
    connect(clearSelectionButton, &QPushButton::clicked, this, &PagePartitionPage::clearSelection);
    connect(redrawButton_, &QPushButton::clicked, this, &PagePartitionPage::redrawSelectedLine);
    connect(deleteButton_, &QPushButton::clicked, this, &PagePartitionPage::deleteSelectedLine);
    connect(undoButton_, &QPushButton::clicked, this, &PagePartitionPage::undoLastChange);
    connect(defaultColorButton_, &QPushButton::clicked, this, &PagePartitionPage::chooseDefaultColor);
    connect(selectedColorButton_, &QPushButton::clicked, this, &PagePartitionPage::chooseSelectedColor);
    connect(widthSpin_, &QSpinBox::valueChanged, this, [this](int value) {
        model_.setDefaultWidth(value);
        commitSettingsChange(QStringLiteral("默认粗细已更新"));
    });
    connect(onlyShowWhenEveDetectedCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        model_.setOnlyShowWhenEveDetected(checked);
        commitSettingsChange(checked
                ? QStringLiteral("已启用 EVE 客户端检测显示")
                : QStringLiteral("已关闭 EVE 客户端检测显示"));
    });
    connect(table_, &QTableWidget::currentCellChanged, this,
        [this](int currentRow, int, int, int) { setSelectedLineFromRow(currentRow); });
    connect(table_, &QTableWidget::cellDoubleClicked, this, &PagePartitionPage::editCell);

    connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen* screen) {
        connect(screen, &QScreen::geometryChanged, this, [this] {
            refreshScreenChoices();
            refreshOverlays();
        });
        refreshScreenChoices();
        refreshOverlays();
    });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen*) {
        refreshScreenChoices();
        refreshOverlays();
    });
    connect(qApp, &QGuiApplication::primaryScreenChanged, this, [this](QScreen*) {
        refreshScreenChoices();
    });
    for (QScreen* screen : QGuiApplication::screens()) {
        connect(screen, &QScreen::geometryChanged, this, [this] {
            refreshScreenChoices();
            refreshOverlays();
        });
    }

    refreshScreenChoices();
    refreshColorButtons();
    refreshTable();
    refreshOverlays();
}

PagePartitionPage::~PagePartitionPage()
{
    if (drawOverlay_ != nullptr) {
        drawOverlay_->close();
    }
    qDeleteAll(overlays_);
    overlays_.clear();
}

const PagePartitionConfig& PagePartitionPage::partitionConfig() const noexcept
{
    return model_.config();
}

void PagePartitionPage::setPartitionConfig(const PagePartitionConfig& config)
{
    if (model_.config() == config) {
        return;
    }
    model_.setConfig(config);
    widthSpin_->setValue(model_.config().defaultWidth);
    {
        const QSignalBlocker blocker(onlyShowWhenEveDetectedCheck_);
        onlyShowWhenEveDetectedCheck_->setChecked(model_.config().onlyShowWhenEveDetected);
    }
    if (selectedLineIndex() < 0) {
        selectedLineId_.clear();
    }
    refreshColorButtons();
    refreshTable();
    refreshOverlays();
}

void PagePartitionPage::setEveClientPresent(bool present)
{
    if (eveClientPresent_ == present) {
        return;
    }
    eveClientPresent_ = present;
    refreshOverlays();
}

void PagePartitionPage::setCaptureOverlaySuppressed(bool suppressed)
{
    if (captureOverlaySuppressed_ == suppressed) {
        return;
    }
    const bool wasSuppressed = captureOverlaySuppressed_ || fishModeSuppressed_;
    if (suppressed) {
        if (!wasSuppressed) {
            drawOverlayWasVisibleBeforeSuppression_ =
                drawOverlay_ != nullptr && drawOverlay_->isVisible();
        }
        if (drawOverlay_ != nullptr && drawOverlay_->isVisible()) {
            drawOverlay_->hide();
        }
    }
    captureOverlaySuppressed_ = suppressed;
    refreshOverlays();
    if (!captureOverlaySuppressed_ && !fishModeSuppressed_
        && drawOverlayWasVisibleBeforeSuppression_ && drawOverlay_ != nullptr) {
        drawOverlay_->show();
    }
    if (!captureOverlaySuppressed_ && !fishModeSuppressed_) {
        drawOverlayWasVisibleBeforeSuppression_ = false;
    }
    refreshScreenChoices();
}

void PagePartitionPage::setFishModeSuppressed(bool suppressed)
{
    if (fishModeSuppressed_ == suppressed) {
        return;
    }
    const bool wasSuppressed = captureOverlaySuppressed_ || fishModeSuppressed_;
    if (suppressed) {
        if (!wasSuppressed) {
            drawOverlayWasVisibleBeforeSuppression_ =
                drawOverlay_ != nullptr && drawOverlay_->isVisible();
        }
        if (drawOverlay_ != nullptr && drawOverlay_->isVisible()) {
            drawOverlay_->hide();
        }
    }
    fishModeSuppressed_ = suppressed;
    refreshOverlays();
    if (!captureOverlaySuppressed_ && !fishModeSuppressed_
        && drawOverlayWasVisibleBeforeSuppression_ && drawOverlay_ != nullptr) {
        drawOverlay_->show();
    }
    if (!captureOverlaySuppressed_ && !fishModeSuppressed_) {
        drawOverlayWasVisibleBeforeSuppression_ = false;
    }
    refreshScreenChoices();
}

void PagePartitionPage::addLine()
{
    if (drawOverlay_ != nullptr || fishModeSuppressed_) {
        return;
    }
    QScreen* screen = selectedScreen();
    if (screen == nullptr) {
        statusLabel_->setText(QStringLiteral("未检测到可用显示器"));
        return;
    }
    screenCombo_->setEnabled(false);
    addButton_->setEnabled(false);
    auto* overlay = new PartitionDrawOverlay(
        screen, model_.config().defaultColor, model_.config().defaultWidth, std::nullopt,
        [this](std::optional<PagePartitionLine> line) {
            drawOverlay_ = nullptr;
            refreshScreenChoices();
            if (!line.has_value()) {
                statusLabel_->setText(QStringLiteral("已取消绘制"));
                return;
            }
            line->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            line->note = QStringLiteral("线条%1").arg(model_.config().lines.size() + 1);
            selectedLineId_ = line->id;
            model_.addLine(std::move(*line));
            commitLineChange(QStringLiteral("已添加分区线"));
        });
    drawOverlay_ = overlay;
    connect(overlay, &QObject::destroyed, this, [this] {
        drawOverlay_ = nullptr;
        refreshScreenChoices();
    });
    overlay->show();
}

void PagePartitionPage::refreshScreenChoices()
{
    QScreen* previousSelection = nullptr;
    const int previousIndex = screenCombo_->currentIndex();
    if (previousIndex >= 0 && previousIndex < screenChoices_.size()) {
        previousSelection = screenChoices_.at(previousIndex).data();
    }

    const QList<QScreen*> availableScreens = QGuiApplication::screens();
    QScreen* primaryScreen = QGuiApplication::primaryScreen();
    QSignalBlocker blocker(screenCombo_);
    screenCombo_->clear();
    screenChoices_.clear();

    int selectedIndex = -1;
    for (int index = 0; index < availableScreens.size(); ++index) {
        QScreen* screen = availableScreens.at(index);
        QString label = QStringLiteral("屏幕 %1").arg(index + 1);
        if (!screen->name().trimmed().isEmpty()) {
            label += QStringLiteral(" · %1").arg(screen->name());
        }
        const QSize size = screen->geometry().size();
        label += QStringLiteral(" · %1 × %2").arg(size.width()).arg(size.height());
        if (screen == primaryScreen) {
            label += QStringLiteral("（主屏）");
        }

        screenChoices_.append(screen);
        screenCombo_->addItem(label);
        if (screen == previousSelection) {
            selectedIndex = index;
        }
    }

    if (selectedIndex < 0 && !availableScreens.isEmpty()) {
        selectedIndex = availableScreens.indexOf(primaryScreen);
        if (selectedIndex < 0) {
            selectedIndex = 0;
        }
    }
    if (selectedIndex >= 0) {
        screenCombo_->setCurrentIndex(selectedIndex);
    }

    const bool canSelectScreen = selectedIndex >= 0 && drawOverlay_ == nullptr
        && !fishModeSuppressed_;
    screenCombo_->setEnabled(canSelectScreen);
    addButton_->setEnabled(canSelectScreen);
}

QScreen* PagePartitionPage::selectedScreen() const noexcept
{
    const int index = screenCombo_->currentIndex();
    if (index < 0 || index >= screenChoices_.size()) {
        return nullptr;
    }
    QScreen* screen = screenChoices_.at(index).data();
    if (screen == nullptr || !QGuiApplication::screens().contains(screen)) {
        return nullptr;
    }
    return screen;
}

void PagePartitionPage::redrawSelectedLine()
{
    const int index = selectedLineIndex();
    if (index < 0 || drawOverlay_ != nullptr || fishModeSuppressed_) {
        return;
    }
    const PagePartitionLine original = model_.config().lines.at(index);
    QScreen* screen = QGuiApplication::screenAt(original.start);
    if (screen == nullptr) {
        screen = QGuiApplication::screenAt(QCursor::pos());
    }
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        return;
    }
    auto* overlay = new PartitionDrawOverlay(
        screen, original.color, original.width, original,
        [this, original](std::optional<PagePartitionLine> line) {
            drawOverlay_ = nullptr;
            refreshScreenChoices();
            if (!line.has_value()) {
                statusLabel_->setText(QStringLiteral("已取消修改"));
                return;
            }
            const int currentIndex = selectedLineIndex();
            if (currentIndex < 0) {
                return;
            }
            if (line->start == original.start && line->end == original.end) {
                statusLabel_->setText(QStringLiteral("线条没有变化"));
                return;
            }
            line->id = original.id;
            line->note = original.note;
            line->color = original.color;
            line->width = original.width;
            model_.updateLine(currentIndex, std::move(*line));
            commitLineChange(QStringLiteral("线条位置已更新"));
        });
    screenCombo_->setEnabled(false);
    addButton_->setEnabled(false);
    drawOverlay_ = overlay;
    connect(overlay, &QObject::destroyed, this, [this] {
        drawOverlay_ = nullptr;
        refreshScreenChoices();
    });
    overlay->show();
}

void PagePartitionPage::deleteSelectedLine()
{
    const int index = selectedLineIndex();
    if (index < 0) {
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("删除线条"),
            QStringLiteral("确定删除“%1”吗？").arg(model_.config().lines.at(index).note))
        != QMessageBox::Yes) {
        return;
    }
    model_.removeLine(index);
    selectedLineId_.clear();
    commitLineChange(QStringLiteral("线条已删除"));
}

void PagePartitionPage::clearAllLines()
{
    if (model_.config().lines.isEmpty()) {
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("清除全部"),
            QStringLiteral("确定清除全部 %1 条分区线吗？").arg(model_.config().lines.size()))
        != QMessageBox::Yes) {
        return;
    }
    model_.clearLines();
    selectedLineId_.clear();
    commitLineChange(QStringLiteral("已清除全部线条"));
}

void PagePartitionPage::undoLastChange()
{
    if (!model_.undo()) {
        return;
    }
    if (selectedLineIndex() < 0) {
        selectedLineId_.clear();
    }
    commitLineChange(QStringLiteral("已撤销上一次改动"));
}

void PagePartitionPage::toggleVisibility()
{
    model_.toggleVisibility();
    commitSettingsChange(model_.config().visible ? QStringLiteral("线条已显示")
                                         : QStringLiteral("线条已隐藏"));
}

void PagePartitionPage::clearSelection()
{
    selectedLineId_.clear();
    table_->clearSelection();
    table_->setCurrentCell(-1, -1);
    refreshOverlays();
    refreshButtons();
    statusLabel_->setText(QStringLiteral("已取消选中"));
}

void PagePartitionPage::chooseDefaultColor()
{
    const QColor color = chooseRgbColor(
        this, model_.config().defaultColor, QStringLiteral("选择默认线条颜色"));
    if (!color.isValid()) {
        return;
    }
    model_.setDefaultColor(color);
    commitSettingsChange(QStringLiteral("默认颜色已更新"));
}

void PagePartitionPage::chooseSelectedColor()
{
    const QColor color = chooseRgbColor(
        this, model_.config().selectedColor, QStringLiteral("选择选中高亮颜色"));
    if (!color.isValid()) {
        return;
    }
    model_.setSelectedColor(color);
    commitSettingsChange(QStringLiteral("选中颜色已更新"));
}

void PagePartitionPage::editCell(int row, int column)
{
    if (row < 0 || row >= model_.config().lines.size()) {
        return;
    }
    selectedLineId_ = model_.config().lines.at(row).id;
    if (column == 1 || column == 5) {
        redrawSelectedLine();
        return;
    }
    PagePartitionLine updated = model_.config().lines.at(row);
    if (column == 2) {
        bool accepted = false;
        const int width = QInputDialog::getInt(
            this, QStringLiteral("编辑粗细"), QStringLiteral("线条粗细"),
            updated.width, kPagePartitionLineWidthMinimum,
            kPagePartitionLineWidthMaximum, 1, &accepted);
        if (!accepted) {
            return;
        }
        updated.width = width;
    } else if (column == 3) {
        const QColor color = chooseRgbColor(
            this, updated.color, QStringLiteral("选择线条颜色"));
        if (!color.isValid()) {
            return;
        }
        updated.color = color;
    } else if (column == 4) {
        bool accepted = false;
        const QString note = QInputDialog::getText(
            this, QStringLiteral("编辑备注"), QStringLiteral("备注"),
            QLineEdit::Normal, updated.note, &accepted).trimmed();
        if (!accepted || note.isEmpty()) {
            return;
        }
        updated.note = note;
    } else {
        return;
    }
    model_.updateLine(row, std::move(updated));
    commitLineChange(QStringLiteral("线条已更新"));
}

void PagePartitionPage::moveLine(int sourceRow, int targetRow)
{
    if (sourceRow < 0 || targetRow < 0 || sourceRow >= model_.config().lines.size()
        || targetRow >= model_.config().lines.size() || sourceRow == targetRow) {
        return;
    }
    const PagePartitionLine line = model_.config().lines.at(sourceRow);
    model_.moveLine(sourceRow, targetRow);
    selectedLineId_ = line.id;
    commitLineChange(QStringLiteral("线条顺序已调整"));
}

void PagePartitionPage::setSelectedLineFromRow(int row)
{
    if (refreshing_) {
        return;
    }
    if (row < 0 || row >= model_.config().lines.size()) {
        selectedLineId_.clear();
    } else {
        selectedLineId_ = model_.config().lines.at(row).id;
        statusLabel_->setText(
            QStringLiteral("已选中：%1").arg(model_.config().lines.at(row).note));
    }
    refreshOverlays();
    refreshButtons();
}


void PagePartitionPage::commitLineChange(const QString& status)
{
    refreshTable();
    refreshOverlays();
    statusLabel_->setText(status);
    emit configurationChanged();
}

void PagePartitionPage::commitSettingsChange(const QString& status)
{
    refreshColorButtons();
    refreshOverlays();
    refreshButtons();
    statusLabel_->setText(status);
    emit configurationChanged();
}

void PagePartitionPage::refreshTable()
{
    refreshing_ = true;
    table_->setRowCount(model_.config().lines.size());
    int selectedRow = -1;
    for (int row = 0; row < model_.config().lines.size(); ++row) {
        const auto& line = model_.config().lines.at(row);
        const QString type = line.isHorizontal() ? QStringLiteral("水平")
                                                 : QStringLiteral("垂直");
        const QString coordinates = QStringLiteral("(%1,%2) → (%3,%4)")
                                        .arg(line.start.x()).arg(line.start.y())
                                        .arg(line.end.x()).arg(line.end.y());
        const QStringList values{
            QString::number(row + 1), type,
            QStringLiteral("%1 px").arg(line.width), line.color.name(),
            line.note, coordinates};
        for (int column = 0; column < values.size(); ++column) {
            auto* item = new QTableWidgetItem(values.at(column));
            item->setData(kLineIdRole, line.id);
            if (column != 4) {
                item->setTextAlignment(Qt::AlignCenter);
            }
            if (column == 3) {
                item->setBackground(line.color);
                item->setForeground(line.color.lightness() > 140 ? Qt::black : Qt::white);
            }
            table_->setItem(row, column, item);
        }
        if (line.id == selectedLineId_) {
            selectedRow = row;
        }
    }
    if (selectedRow >= 0) {
        table_->selectRow(selectedRow);
        table_->setCurrentCell(selectedRow, 0);
    } else {
        table_->clearSelection();
    }
    refreshing_ = false;
    refreshButtons();
}

void PagePartitionPage::refreshOverlays()
{
    qDeleteAll(overlays_);
    overlays_.clear();
    if (captureOverlaySuppressed_ || fishModeSuppressed_
        || !model_.config().visible || model_.config().lines.isEmpty()
        || (model_.config().onlyShowWhenEveDetected && !eveClientPresent_)) {
        return;
    }
    for (QScreen* screen : QGuiApplication::screens()) {
        auto* overlay = new PartitionOverlay(screen->geometry());
        overlay->setLines(model_.config().lines, selectedLineId_, model_.config().selectedColor);
        overlays_.append(overlay);
        overlay->show();
    }
}

void PagePartitionPage::refreshButtons()
{
    const bool hasSelection = selectedLineIndex() >= 0;
    redrawButton_->setEnabled(hasSelection && !fishModeSuppressed_);
    deleteButton_->setEnabled(hasSelection);
    undoButton_->setEnabled(model_.canUndo());
    visibilityButton_->setText(
        model_.config().visible ? QStringLiteral("隐藏线条") : QStringLiteral("显示线条"));
}

void PagePartitionPage::refreshColorButtons()
{
    defaultColorButton_->setText(model_.config().defaultColor.name());
    defaultColorButton_->setStyleSheet(swatchStyle(model_.config().defaultColor));
    selectedColorButton_->setText(model_.config().selectedColor.name());
    selectedColorButton_->setStyleSheet(swatchStyle(model_.config().selectedColor));
}

int PagePartitionPage::selectedLineIndex() const
{
    if (selectedLineId_.isEmpty()) {
        return -1;
    }
    for (int index = 0; index < model_.config().lines.size(); ++index) {
        if (model_.config().lines.at(index).id == selectedLineId_) {
            return index;
        }
    }
    return -1;
}
