#include "calibrationprofile.h"

#include <QtMath>

#include <cmath>

CalibrationAlgorithm CalibrationProfile::algorithm() const
{
    return id == QStringLiteral("ECM1A2000") ? CalibrationAlgorithm::LookupTable
                                             : CalibrationAlgorithm::LinearFit;
}

double CalibrationProfile::toleranceN() const
{
    return static_cast<double>(ratedMaxForceN) * tolerancePercent / 100.0;
}

QList<CalibrationProfile> standardCalibrationProfiles()
{
    CalibrationProfile force2000;
    force2000.id = QStringLiteral("ECM1A2000");
    force2000.displayName = QStringLiteral("ECM1A2000");
    force2000.ratedMaxForceN = 2000;
    force2000.calibrationMaxForceN = 2000;
    force2000.stepN = 200;
    force2000.tolerancePercent = 1.5;
    force2000.allowsNarrowRange = false;

    CalibrationProfile force200;
    force200.id = QStringLiteral("ECM1A200");
    force200.displayName = QStringLiteral("ECM1A200");
    force200.ratedMaxForceN = 200;
    force200.calibrationMaxForceN = 200;
    force200.stepN = 20;
    force200.tolerancePercent = 1.5;
    force200.allowsNarrowRange = true;

    CalibrationProfile force500;
    force500.id = QStringLiteral("ECM1A500");
    force500.displayName = QStringLiteral("ECM1A500");
    force500.ratedMaxForceN = 500;
    force500.calibrationMaxForceN = 500;
    force500.stepN = 50;
    force500.tolerancePercent = 1.5;
    force500.allowsNarrowRange = true;

    return {force2000, force200, force500};
}

CalibrationProfile calibrationProfileById(const QString &id)
{
    const QList<CalibrationProfile> profiles = standardCalibrationProfiles();
    for (const CalibrationProfile &profile : profiles) {
        if (profile.id == id) {
            return profile;
        }
    }
    return profiles.first();
}

int defaultCalibrationStep(int maxForceN)
{
    return qMax(1, qRound(static_cast<double>(maxForceN) / 10.0));
}

QVector<int> buildCalibrationForcePoints(int maxForceN, int stepN)
{
    QVector<int> points;
    if (maxForceN <= 0 || stepN <= 0) {
        return points;
    }

    points.reserve(maxForceN / stepN + 2);
    points.append(0);
    for (qint64 force = stepN; force <= maxForceN; force += stepN) {
        points.append(static_cast<int>(force));
    }
    if (points.last() != maxForceN) {
        points.append(maxForceN);
    }
    return points;
}

LinearFitResult fitLinearForward(const QVector<QPointF> &points)
{
    LinearFitResult result;
    if (points.size() < 2) {
        return result;
    }

    double sumX = 0.0;
    double sumY = 0.0;
    double sumXX = 0.0;
    double sumXY = 0.0;
    for (const QPointF &point : points) {
        sumX += point.x();
        sumY += point.y();
        sumXX += point.x() * point.x();
        sumXY += point.x() * point.y();
    }

    const double count = static_cast<double>(points.size());
    const double denominator = count * sumXX - sumX * sumX;
    if (qAbs(denominator) < 1.0e-12) {
        return result;
    }

    const double k = (count * sumXY - sumX * sumY) / denominator;
    const double b = (sumY - k * sumX) / count;
    if (!std::isfinite(k) || !std::isfinite(b) || qAbs(k) < 1.0e-12) {
        return result;
    }

    const double meanY = sumY / count;
    double totalVariance = 0.0;
    double squaredError = 0.0;
    double maxError = 0.0;
    for (const QPointF &point : points) {
        const double predicted = k * point.x() + b;
        const double error = predicted - point.y();
        squaredError += error * error;
        maxError = qMax(maxError, qAbs(error));
        const double centered = point.y() - meanY;
        totalVariance += centered * centered;
    }

    result.valid = true;
    result.k = k;
    result.b = b;
    result.maxErrorN = maxError;
    result.rmsErrorN = std::sqrt(squaredError / count);
    result.rSquared = totalVariance < 1.0e-12
        ? (squaredError < 1.0e-12 ? 1.0 : 0.0)
        : 1.0 - squaredError / totalVariance;
    return result;
}

int calibrationPn168FromK(double k)
{
    return qRound(k * 10.0);
}
