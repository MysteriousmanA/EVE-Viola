#include "ui/desktop_pet_page.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QTableWidget>
#include <QVBoxLayout>
#include "ui/desktop_pet/avatar_crop_widget.h"
#include "ui/desktop_pet/avatar_preview_widget.h"

#include "ui/desktop_pet/desktop_pet_constants.h"

using namespace DesktopPetUi;

namespace {

QLabel* makeSectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("petSectionTitle"));
    return label;
}

QLabel* makeMutedText(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("muted"));
    label->setWordWrap(true);
    return label;
}

} // namespace

std::pair<QLabel*, QLabel*> DesktopPetPage::buildLayout()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("desktopPetPageScrollArea"));
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    rootLayout->addWidget(scrollArea);
    auto* content = new QWidget(scrollArea);
    scrollArea->setWidget(content);
    auto* pageLayout = new QVBoxLayout(content);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(10);

    auto* title = new QLabel(QStringLiteral("摸鱼桌宠"), content);
    title->setObjectName(QStringLiteral("sectionTitle"));
    auto* description = makeMutedText(
        QStringLiteral("上传图片并裁成圆形头像放在桌面上。启用后桌宠会在程序下次启动时自动显示；左键拖动头像时轮盘会一起移动，再次单击头像可关闭轮盘。右键菜单可开启摸鱼模式，隐藏小窗与分区线、暂停图色音频和自动切换。"),
        content);
    pageLayout->addWidget(title);
    pageLayout->addWidget(description);

    auto* surface = new QFrame(content);
    surface->setObjectName(QStringLiteral("desktopPetEditorPanel"));
    auto* surfaceLayout = new QHBoxLayout(surface);
    surfaceLayout->setContentsMargins(18, 16, 18, 16);
    surfaceLayout->setSpacing(20);

    auto* editor = new QWidget(surface);
    auto* editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(10);
    auto* imageRow = new QHBoxLayout;
    uploadButton_ = new QPushButton(QStringLiteral("选择图片"), editor);
    uploadButton_->setObjectName(QStringLiteral("desktopPetUploadImage"));
    imageNameLabel_ = makeMutedText(QStringLiteral("支持 PNG、JPG、BMP 和 WebP"), editor);
    imageNameLabel_->setObjectName(QStringLiteral("desktopPetImageName"));
    imageRow->addWidget(uploadButton_);
    imageRow->addWidget(imageNameLabel_, 1);
    editorLayout->addLayout(imageRow);

    cropWidget_ = new AvatarCropWidget(editor);
    auto* cropRow = new QHBoxLayout;
    cropRow->addStretch(1);
    cropRow->addWidget(cropWidget_);
    cropRow->addStretch(1);
    editorLayout->addLayout(cropRow, 1);

    auto* zoomRow = new QHBoxLayout;
    auto* zoomLabel = new QLabel(QStringLiteral("缩放"), editor);
    zoomLabel->setObjectName(QStringLiteral("muted"));
    zoomSlider_ = new QSlider(Qt::Horizontal, editor);
    zoomSlider_->setObjectName(QStringLiteral("desktopPetZoom"));
    zoomSlider_->setRange(100, 300);
    zoomSlider_->setValue(100);
    auto* zoomValue = new QLabel(QStringLiteral("100%"), editor);
    zoomValue->setObjectName(QStringLiteral("desktopPetZoomValue"));
    zoomValue->setMinimumWidth(42);
    zoomRow->addWidget(zoomLabel);
    zoomRow->addWidget(zoomSlider_, 1);
    zoomRow->addWidget(zoomValue);
    editorLayout->addLayout(zoomRow);
    surfaceLayout->addWidget(editor, 3);

    auto* settings = new QWidget(surface);
    auto* settingsLayout = new QVBoxLayout(settings);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(9);
    settingsLayout->addWidget(makeSectionTitle(QStringLiteral("桌宠外观"), settings));
    previewWidget_ = new AvatarPreviewWidget(settings);
    auto* previewRow = new QHBoxLayout;
    previewRow->addStretch(1);
    previewRow->addWidget(previewWidget_);
    previewRow->addStretch(1);
    settingsLayout->addLayout(previewRow);

    auto* sizeLabel = new QLabel(QStringLiteral("桌宠大小"), settings);
    sizeLabel->setObjectName(QStringLiteral("muted"));
    settingsLayout->addWidget(sizeLabel);
    auto* sizeRow = new QHBoxLayout;
    sizeSlider_ = new QSlider(Qt::Horizontal, settings);
    sizeSlider_->setObjectName(QStringLiteral("desktopPetSize"));
    sizeSlider_->setRange(kPetSizeMinimum, kPetSizeMaximum);
    sizeSlider_->setValue(128);
    auto* sizeValue = new QLabel(QStringLiteral("128 px"), settings);
    sizeValue->setObjectName(QStringLiteral("desktopPetSizeValue"));
    sizeValue->setMinimumWidth(54);
    sizeRow->addWidget(sizeSlider_, 1);
    sizeRow->addWidget(sizeValue);
    settingsLayout->addLayout(sizeRow);

    auto* interactionHint = makeMutedText(
        QStringLiteral("桌宠会保持在其他窗口上方。账号昵称与扇区顺序沿用“快捷切屏”的“账号圆盘切换”设置。左键拖动头像时圆环会一起移动，滚轮调整大小；单击头像打开或关闭轮盘，点选圆环扇区切号，右键圆环也可关闭。"), settings);
    settingsLayout->addWidget(interactionHint);
    statusLabel_ = makeMutedText(QStringLiteral("还没有头像，先选择一张图片。"), settings);
    statusLabel_->setObjectName(QStringLiteral("desktopPetStatus"));
    settingsLayout->addWidget(statusLabel_);
    petButton_ = new QPushButton(QStringLiteral("启用桌宠"), settings);
    petButton_->setObjectName(QStringLiteral("desktopPetToggle"));
    petButton_->setProperty("accent", true);
    petButton_->setEnabled(false);
    settingsLayout->addWidget(petButton_);
    settingsLayout->addStretch(1);
    surfaceLayout->addWidget(settings, 2);
    pageLayout->addWidget(surface);

    pageLayout->addWidget(makeSectionTitle(QStringLiteral("图色逻辑触发文案"), content));
    pageLayout->addWidget(makeMutedText(
        QStringLiteral("为每条规则填写最多 120 字的桌宠提示；触发账号名会显示在文案前，气泡位于头像下方。多个账号连续触发时会一并显示。留空则不显示文案；桌宠隐藏时会临时出现，提示结束后自动收起。"),
        content));
    rulesEmptyState_ = makeMutedText(
        QStringLiteral("还没有图色逻辑规则，请先在“图色逻辑”页新增规则。"), content);
    rulesEmptyState_->setObjectName(QStringLiteral("desktopPetRulesEmptyState"));
    pageLayout->addWidget(rulesEmptyState_);
    rulesTable_ = new QTableWidget(0, 2, content);
    rulesTable_->setObjectName(QStringLiteral("desktopPetRuleMessages"));
    rulesTable_->setHorizontalHeaderLabels({
        QStringLiteral("图色逻辑规则"), QStringLiteral("触发时显示文案")});
    rulesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    rulesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    rulesTable_->verticalHeader()->hide();
    rulesTable_->setSelectionMode(QAbstractItemView::NoSelection);
    rulesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rulesTable_->setAlternatingRowColors(true);
    rulesTable_->setMinimumHeight(104);
    rulesTable_->setMaximumHeight(264);
    rulesTable_->hide();
    pageLayout->addWidget(rulesTable_);
    pageLayout->addStretch(1);
    return {zoomValue, sizeValue};
}
