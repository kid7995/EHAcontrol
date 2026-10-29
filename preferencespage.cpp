#include "preferencespage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

PreferencesPage::PreferencesPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("应用配置"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("配置启动画面、默认工作页面和界面语言。"), this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    root->addWidget(title);
    root->addWidget(subtitle);

    auto *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(22, 20, 22, 20);
    auto *form = new QFormLayout;
    m_showSplash = new QCheckBox(QStringLiteral("启动时显示启动画面"), panel);
    m_defaultPage = new QComboBox(panel);
    m_defaultPage->addItem(QStringLiteral("实时设备概览"), 0);
    m_defaultPage->addItem(QStringLiteral("参数设定"), 1);
    m_defaultPage->addItem(QStringLiteral("实时示波器"), 2);
    m_defaultPage->addItem(QStringLiteral("串口通信"), 3);
    m_defaultPage->addItem(QStringLiteral("驱动器信息"), 4);
    m_language = new QComboBox(panel);
    m_language->addItem(QStringLiteral("简体中文"), QStringLiteral("zh_CN"));
    m_language->addItem(QStringLiteral("English（界面文本待翻译）"), QStringLiteral("en_US"));
    form->addRow(QStringLiteral("启动画面"), m_showSplash);
    form->addRow(QStringLiteral("默认页面"), m_defaultPage);
    form->addRow(QStringLiteral("界面语言"), m_language);
    layout->addLayout(form);
    auto *saveButton = new QPushButton(QStringLiteral("保存配置"), panel);
    saveButton->setObjectName(QStringLiteral("primaryButton"));
    layout->addWidget(saveButton, 0, Qt::AlignLeft);
    m_status = new QLabel(panel);
    m_status->setObjectName(QStringLiteral("mutedText"));
    layout->addWidget(m_status);
    root->addWidget(panel);
    root->addStretch();

    QSettings settings;
    m_showSplash->setChecked(settings.value(QStringLiteral("ui/showSplash"), false).toBool());
    const int defaultPage = settings.value(QStringLiteral("ui/defaultPage"), 0).toInt();
    const int pageIndex = m_defaultPage->findData(defaultPage);
    if (pageIndex >= 0) {
        m_defaultPage->setCurrentIndex(pageIndex);
    }
    const QString language = settings.value(QStringLiteral("ui/language"),
                                             QStringLiteral("zh_CN")).toString();
    const int languageIndex = m_language->findData(language);
    if (languageIndex >= 0) {
        m_language->setCurrentIndex(languageIndex);
    }

    connect(saveButton, &QPushButton::clicked, this, &PreferencesPage::saveSettings);
}

void PreferencesPage::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/showSplash"), m_showSplash->isChecked());
    settings.setValue(QStringLiteral("ui/defaultPage"), m_defaultPage->currentData().toInt());
    settings.setValue(QStringLiteral("ui/language"), m_language->currentData().toString());
    m_status->setText(QStringLiteral("配置已保存，默认页面将在下次启动时生效。"));
    emit settingsSaved(m_defaultPage->currentData().toInt(),
                       m_showSplash->isChecked());
}
