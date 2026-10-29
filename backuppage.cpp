#include "backuppage.h"

#include "parametermodel.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QFrame *flowCard(const QString &imagePath, const QString &title,
                 const QString &description, QWidget *parent,
                 QLabel *status)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(10);
    auto *image = new QLabel(card);
    image->setPixmap(QPixmap(imagePath).scaled(
        180, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    image->setAlignment(Qt::AlignCenter);
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    auto *descriptionLabel = new QLabel(description, card);
    descriptionLabel->setObjectName(QStringLiteral("mutedText"));
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(image, 0, Qt::AlignCenter);
    layout->addWidget(titleLabel);
    layout->addWidget(descriptionLabel);
    layout->addStretch();
    Q_UNUSED(status)
    return card;
}

} // namespace

BackupPage::BackupPage(ParameterModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("参数备份与恢复"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("将设备全部 Pn 参数保存为文件，或从文件恢复并写回设备。"
                       "恢复前请先读取设备参数并确认机型与固件一致。"),
        this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    subtitle->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(subtitle);

    auto *flowRow = new QHBoxLayout;
    flowRow->setSpacing(14);

    auto *backupCard = flowCard(
        QStringLiteral(":/reference/images/backup.png"),
        QStringLiteral("设备 → 文件"),
        QStringLiteral("先从设备读取 Pn000–Pn199，再导出参考版兼容的 TXT 备份。"),
        this, m_status);
    auto *backupLayout = qobject_cast<QVBoxLayout *>(backupCard->layout());
    auto *readButton = new QPushButton(QStringLiteral("1. 读取全部设备参数"), backupCard);
    readButton->setObjectName(QStringLiteral("secondaryButton"));
    auto *exportButton = new QPushButton(QStringLiteral("2. 导出备份文件"), backupCard);
    exportButton->setObjectName(QStringLiteral("primaryButton"));
    backupLayout->insertWidget(backupLayout->count() - 1, readButton);
    backupLayout->insertWidget(backupLayout->count() - 1, exportButton);
    flowRow->addWidget(backupCard, 1);

    auto *restoreCard = flowCard(
        QStringLiteral(":/reference/images/restore.png"),
        QStringLiteral("文件 → 设备"),
        QStringLiteral("导入参数文件后，参数进入待写入状态，再确认批量写入设备。"),
        this, m_status);
    auto *restoreLayout = qobject_cast<QVBoxLayout *>(restoreCard->layout());
    auto *importButton = new QPushButton(QStringLiteral("1. 导入备份文件"), restoreCard);
    importButton->setObjectName(QStringLiteral("secondaryButton"));
    auto *writeButton = new QPushButton(QStringLiteral("2. 写入设备"), restoreCard);
    writeButton->setObjectName(QStringLiteral("dangerButton"));
    restoreLayout->insertWidget(restoreLayout->count() - 1, importButton);
    restoreLayout->insertWidget(restoreLayout->count() - 1, writeButton);
    flowRow->addWidget(restoreCard, 1);
    root->addLayout(flowRow, 1);

    auto *note = new QLabel(
        QStringLiteral("TXT 使用参考版原始值格式：每行“PnNNN 原始值”，例如 Pn000 9999。"
                       "原始值不做工程单位换算，可无损写回设备。"
                       "恢复后建议重新读取一次并核对关键参数，再执行 Pn152 保存。"),
        this);
    note->setObjectName(QStringLiteral("fieldHint"));
    note->setWordWrap(true);
    root->addWidget(note);
    m_status = new QLabel(QStringLiteral("尚未读取设备参数"), this);
    m_status->setObjectName(QStringLiteral("mutedText"));
    root->addWidget(m_status);

    connect(readButton, &QPushButton::clicked, this, &BackupPage::readAllRequested);
    connect(exportButton, &QPushButton::clicked, this, &BackupPage::exportBackup);
    connect(importButton, &QPushButton::clicked, this, &BackupPage::importBackup);
    connect(writeButton, &QPushButton::clicked, this, &BackupPage::writeDirtyRequested);
    setConnected(false);
}

void BackupPage::setConnected(bool connected)
{
    m_connected = connected;
}

void BackupPage::handleParameterReadFinished(bool complete, int received)
{
    m_status->setText(
        complete ? QStringLiteral("设备参数已读取：%1 项，可以导出备份").arg(received)
                 : QStringLiteral("参数读取未完成：%1 项，不建议导出").arg(received));
}

void BackupPage::exportBackup()
{
    if (m_model->confirmedCount() == 0) {
        QMessageBox::information(this, QStringLiteral("导出备份"),
                                 QStringLiteral("请先读取设备参数"));
        return;
    }
    const QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出参数备份"),
        QStringLiteral("KaiserStudio_Parameters.txt"),
        QStringLiteral("参数文本 (*.txt)"));
    if (fileName.isEmpty()) {
        return;
    }
    QString error;
    const bool ok = m_model->exportDriveText(fileName, &error);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), error);
        return;
    }
    m_status->setText(QStringLiteral("备份已导出：%1").arg(fileName));
    emit statusMessage(m_status->text());
}

void BackupPage::importBackup()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入参数备份"), QString(),
        QStringLiteral("参数文本 (*.txt);;旧版 CSV (*.csv);;所有文件 (*.*)"));
    if (fileName.isEmpty()) {
        return;
    }
    QString error;
    const bool textMode =
        QFileInfo(fileName).suffix().compare(QStringLiteral("txt"), Qt::CaseInsensitive) == 0;
    const bool ok = textMode ? m_model->importDriveText(fileName, &error)
                             : m_model->importCsv(fileName, &error);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), error);
        return;
    }
    m_status->setText(
        QStringLiteral("参数已导入，待写入 %1 项").arg(m_model->dirtyCount()));
    emit statusMessage(m_status->text());
}
