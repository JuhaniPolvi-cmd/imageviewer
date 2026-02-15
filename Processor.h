#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <QObject>
#include <QImage>

class Processor : public QObject
{
    Q_OBJECT
public:
    explicit Processor(QObject *parent = nullptr);

signals:
    void internalGenerationTrigger(void);
    void postNewGenerationImage(const QImage &image);

public slots:
    const QImage &PreprocessImage(const QImage &image);
    void triggerGenerationCalculation();

private slots:
    void performGenerationCalculation();

private:
    int cellState(const QImage &image, int x, int y);

    bool isBusy; // Flag used to prevent retriggering new generation calculation before finishing one calculation
    QImage localImageCopy; // Copy of original image converted to black and white, used in generation calculation
};

#endif // PROCESSOR_H
