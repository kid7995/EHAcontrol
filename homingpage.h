#ifndef HOMINGPAGE_H
#define HOMINGPAGE_H

#include <QWidget>

class QComboBox;
class QLabel;
class QScrollArea;

class HomingPage : public QWidget
{
    Q_OBJECT

public:
    explicit HomingPage(QWidget *parent = nullptr);

private slots:
    void updateMethod();

private:
    void addMethod(int value, const QString &description);
    QString descriptionForMethod(int method) const;

    QComboBox *m_methodCombo = nullptr;
    QLabel *m_description = nullptr;
    QLabel *m_image = nullptr;
    QLabel *m_textImage = nullptr;
    QScrollArea *m_scroll = nullptr;
};

#endif // HOMINGPAGE_H
