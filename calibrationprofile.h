#ifndef CALIBRATIONPROFILE_H
#define CALIBRATIONPROFILE_H

#include <QList>
#include <QPointF>
#include <QString>
#include <QVector>

enum class CalibrationAlgorithm
{
    LookupTable,
    LinearFit
};

struct CalibrationProfile
{
    QString id;
    QString displayName;
    int ratedMaxForceN = 0;
    int calibrationMaxForceN = 0;
    int stepN = 0;
    double tolerancePercent = 1.5;
    bool allowsNarrowRange = false;

    CalibrationAlgorithm algorithm() const;
    double toleranceN() const;
};

struct LinearFitResult
{
    bool valid = false;
    double k = 0.0;
    double b = 0.0;
    double rSquared = 0.0;
    double maxErrorN = 0.0;
    double rmsErrorN = 0.0;
};

QList<CalibrationProfile> standardCalibrationProfiles();
CalibrationProfile calibrationProfileById(const QString &id);
int defaultCalibrationStep(int maxForceN);
QVector<int> buildCalibrationForcePoints(int maxForceN, int stepN);
LinearFitResult fitLinearForward(const QVector<QPointF> &points);
int calibrationPn168FromK(double k);

#endif // CALIBRATIONPROFILE_H
