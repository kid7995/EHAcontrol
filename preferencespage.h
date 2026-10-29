#ifndef PREFERENCESPAGE_H
#define PREFERENCESPAGE_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;

class PreferencesPage : public QWidget
{
    Q_OBJECT

public:
    explicit PreferencesPage(QWidget *parent = nullptr);

signals:
    void settingsSaved(int defaultPage, bool showSplash);

private slots:
    void saveSettings();

private:
    QCheckBox *m_showSplash = nullptr;
    QComboBox *m_defaultPage = nullptr;
    QComboBox *m_language = nullptr;
    QLabel *m_status = nullptr;
};

#endif // PREFERENCESPAGE_H
